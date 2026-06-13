#include "BSP.h"
#include <fstream>
#include <cstring>
#include <cassert>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cstdio>

namespace OS {

// ─── Helpers ──────────────────────────────────────────────────────────────────

template<typename T>
static const T* LumpPtr(const std::vector<uint8_t>& data, const BSPRawLump& lump) {
    return reinterpret_cast<const T*>(data.data() + lump.offset);
}

template<typename T>
static size_t LumpCount(const BSPRawLump& lump) {
    return lump.length / sizeof(T);
}

// ─── Load ─────────────────────────────────────────────────────────────────────

BSPFile BSPLoader::Load(const std::string& path) {
    BSPFile bsp;

    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        bsp.error = "Cannot open file: " + path;
        return bsp;
    }

    auto size = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);

    if (data.size() < sizeof(BSPRawHeader)) {
        bsp.error = "File too small to be a BSP";
        return bsp;
    }

    const auto* hdr = reinterpret_cast<const BSPRawHeader*>(data.data());

    if (hdr->version != BSP_VERSION) {
        bsp.error = "Unsupported BSP version: " + std::to_string(hdr->version)
                  + " (expected " + std::to_string(BSP_VERSION) + ")";
        return bsp;
    }

    // Validate all lump boundaries
    for (int i = 0; i < BSP_HEADER_LUMPS; i++) {
        const auto& lump = hdr->lumps[i];
        if (lump.offset < 0 || lump.length < 0) {
            bsp.error = "Invalid lump " + std::to_string(i);
            return bsp;
        }
        if (lump.length > 0 &&
            static_cast<size_t>(lump.offset + lump.length) > data.size()) {
            bsp.error = "Lump " + std::to_string(i) + " extends past end of file";
            return bsp;
        }
    }

    ParseEntities  (bsp, data, hdr->lumps[BSP_LUMP_ENTITIES]);
    ParsePlanes    (bsp, data, hdr->lumps[BSP_LUMP_PLANES]);
    ParseTextures  (bsp, data, hdr->lumps[BSP_LUMP_TEXTURES]);
    ParseVertices  (bsp, data, hdr->lumps[BSP_LUMP_VERTICES]);
    ParseVisibility(bsp, data, hdr->lumps[BSP_LUMP_VISIBILITY]);
    ParseNodes     (bsp, data, hdr->lumps[BSP_LUMP_NODES]);
    ParseTexInfo   (bsp, data, hdr->lumps[BSP_LUMP_TEXINFO]);
    ParseFaces     (bsp, data, hdr->lumps[BSP_LUMP_FACES]);
    ParseLighting  (bsp, data, hdr->lumps[BSP_LUMP_LIGHTING]);
    ParseClipNodes (bsp, data, hdr->lumps[BSP_LUMP_CLIPNODES]);
    ParseLeaves    (bsp, data, hdr->lumps[BSP_LUMP_LEAVES]);
    ParseMarkSurfaces(bsp, data, hdr->lumps[BSP_LUMP_MARKSURFACES]);
    ParseEdges     (bsp, data, hdr->lumps[BSP_LUMP_EDGES]);
    ParseSurfEdges (bsp, data, hdr->lumps[BSP_LUMP_SURFEDGES]);
    ParseModels    (bsp, data, hdr->lumps[BSP_LUMP_MODELS]);

    ExtractWadList(bsp);
    BuildMeshes(bsp);

    bsp.valid = true;
    return bsp;
}

// ─── Parse lumps ──────────────────────────────────────────────────────────────

void BSPLoader::ParseEntities(BSPFile& bsp, const std::vector<uint8_t>& data,
                               const BSPRawLump& lump) {
    if (lump.length == 0) return;
    bsp.entities.assign(
        reinterpret_cast<const char*>(data.data() + lump.offset),
        lump.length
    );
    // Strip null terminator if present
    if (!bsp.entities.empty() && bsp.entities.back() == '\0')
        bsp.entities.pop_back();
}

void BSPLoader::ParsePlanes(BSPFile& bsp, const std::vector<uint8_t>& data,
                             const BSPRawLump& lump) {
    const auto* raw = LumpPtr<BSPRawPlane>(data, lump);
    size_t n = LumpCount<BSPRawPlane>(lump);
    bsp.planes.reserve(n);
    for (size_t i = 0; i < n; i++) {
        bsp.planes.push_back({
            { raw[i].normal[0], raw[i].normal[1], raw[i].normal[2] },
            raw[i].dist
        });
    }
}

void BSPLoader::ParseTextures(BSPFile& bsp, const std::vector<uint8_t>& data,
                               const BSPRawLump& lump) {
    if (lump.length == 0) return;

    const uint8_t* base = data.data() + lump.offset;
    const auto* hdr = reinterpret_cast<const BSPRawMipTexHeader*>(base);
    int32_t numTex = hdr->num_miptex;

    bsp.textures.resize(numTex);

    for (int32_t i = 0; i < numTex; i++) {
        int32_t ofs = hdr->offsets[i];
        if (ofs < 0) {
            // Null texture slot
            bsp.textures[i].name = "";
            continue;
        }

        const auto* miptex = reinterpret_cast<const BSPRawMipTex*>(base + ofs);
        BSPTexture& tex = bsp.textures[i];
        tex.name   = std::string(miptex->name, strnlen(miptex->name, 16));
        tex.width  = miptex->width;
        tex.height = miptex->height;

        // Embedded texture: offsets[0] != 0
        if (miptex->offsets[0] != 0) {
            tex.embedded = true;
            uint32_t mip0size = miptex->width * miptex->height;
            const uint8_t* pixels = reinterpret_cast<const uint8_t*>(miptex) + miptex->offsets[0];
            // After mip0, mip1, mip2, mip3 comes 2-byte padding then 256*3 palette
            uint32_t mip1 = mip0size / 4;
            uint32_t mip2 = mip1 / 4;
            uint32_t mip3 = mip2 / 4;
            const uint8_t* palPtr = pixels + mip0size + mip1 + mip2 + mip3 + 2;
            // Expand indexed to RGBA8
            tex.pixels.resize(mip0size * 4);
            for (uint32_t p = 0; p < mip0size; p++) {
                uint8_t idx = pixels[p];
                tex.pixels[p*4+0] = palPtr[idx*3+0];
                tex.pixels[p*4+1] = palPtr[idx*3+1];
                tex.pixels[p*4+2] = palPtr[idx*3+2];
                // Index 255 = transparent in {BLUE}key textures
                tex.pixels[p*4+3] = (tex.name[0]=='{' && idx==255) ? 0 : 255;
            }
        } else {
            tex.embedded = false;
            tex.wadName  = tex.name;
        }
    }
}

void BSPLoader::ParseVertices(BSPFile& bsp, const std::vector<uint8_t>& data,
                               const BSPRawLump& lump) {
    const float* raw = LumpPtr<float>(data, lump);
    size_t n = lump.length / (3 * sizeof(float));
    bsp.vertices.reserve(n);
    for (size_t i = 0; i < n; i++)
        bsp.vertices.push_back({ raw[i*3], raw[i*3+1], raw[i*3+2] });
}

void BSPLoader::ParseNodes(BSPFile& bsp, const std::vector<uint8_t>& data,
                            const BSPRawLump& lump) {
    const auto* raw = LumpPtr<BSPRawNode>(data, lump);
    size_t n = LumpCount<BSPRawNode>(lump);
    bsp.nodes.reserve(n);
    for (size_t i = 0; i < n; i++) {
        BSPNode node;
        node.plane       = bsp.planes[raw[i].plane];
        node.children[0] = raw[i].children[0];
        node.children[1] = raw[i].children[1];
        node.bounds.mins = { (float)raw[i].mins[0], (float)raw[i].mins[1], (float)raw[i].mins[2] };
        node.bounds.maxs = { (float)raw[i].maxs[0], (float)raw[i].maxs[1], (float)raw[i].maxs[2] };
        node.firstFace   = raw[i].firstface;
        node.numFaces    = raw[i].numfaces;
        bsp.nodes.push_back(node);
    }
}

void BSPLoader::ParseTexInfo(BSPFile& bsp, const std::vector<uint8_t>& data,
                              const BSPRawLump& lump) {
    const auto* raw = LumpPtr<BSPRawTexInfo>(data, lump);
    size_t n = LumpCount<BSPRawTexInfo>(lump);
    bsp.texInfos.assign(raw, raw + n);
}

void BSPLoader::ParseFaces(BSPFile& bsp, const std::vector<uint8_t>& data,
                            const BSPRawLump& lump) {
    const auto* raw = LumpPtr<BSPRawFace>(data, lump);
    size_t n = LumpCount<BSPRawFace>(lump);
    bsp.rawFaces.assign(raw, raw + n);
}

void BSPLoader::ParseLighting(BSPFile& bsp, const std::vector<uint8_t>& data,
                               const BSPRawLump& lump) {
    if (lump.length == 0) return;
    const uint8_t* raw = data.data() + lump.offset;
    bsp.lighting.assign(raw, raw + lump.length);
}

void BSPLoader::ParseClipNodes(BSPFile& bsp, const std::vector<uint8_t>& data,
                                const BSPRawLump& lump) {
    const auto* raw = LumpPtr<BSPRawClipNode>(data, lump);
    size_t n = LumpCount<BSPRawClipNode>(lump);
    bsp.clipNodes.reserve(n);
    for (size_t i = 0; i < n; i++) {
        BSPClipNode cn;
        cn.plane       = bsp.planes[raw[i].plane];
        cn.children[0] = raw[i].children[0];
        cn.children[1] = raw[i].children[1];
        bsp.clipNodes.push_back(cn);
    }
}

void BSPLoader::ParseLeaves(BSPFile& bsp, const std::vector<uint8_t>& data,
                             const BSPRawLump& lump) {
    const auto* raw = LumpPtr<BSPRawLeaf>(data, lump);
    size_t n = LumpCount<BSPRawLeaf>(lump);
    bsp.leaves.reserve(n);
    for (size_t i = 0; i < n; i++) {
        BSPLeaf leaf;
        leaf.contents         = raw[i].contents;
        leaf.visofs           = raw[i].visofs;
        leaf.bounds.mins      = { (float)raw[i].mins[0], (float)raw[i].mins[1], (float)raw[i].mins[2] };
        leaf.bounds.maxs      = { (float)raw[i].maxs[0], (float)raw[i].maxs[1], (float)raw[i].maxs[2] };
        leaf.firstMarkSurface = raw[i].firstmarksurface;
        leaf.numMarkSurfaces  = raw[i].nummarksurfaces;
        memcpy(leaf.ambientLevel, raw[i].ambient_level, 4);
        bsp.leaves.push_back(leaf);
    }
}

void BSPLoader::ParseMarkSurfaces(BSPFile& bsp, const std::vector<uint8_t>& data,
                                   const BSPRawLump& lump) {
    const uint16_t* raw = LumpPtr<uint16_t>(data, lump);
    size_t n = lump.length / sizeof(uint16_t);
    bsp.markSurfaces.assign(raw, raw + n);
}

void BSPLoader::ParseEdges(BSPFile& bsp, const std::vector<uint8_t>& data,
                            const BSPRawLump& lump) {
    const auto* raw = LumpPtr<BSPRawEdge>(data, lump);
    size_t n = LumpCount<BSPRawEdge>(lump);
    bsp.edges.assign(raw, raw + n);
}

void BSPLoader::ParseSurfEdges(BSPFile& bsp, const std::vector<uint8_t>& data,
                                const BSPRawLump& lump) {
    const int32_t* raw = LumpPtr<int32_t>(data, lump);
    size_t n = lump.length / sizeof(int32_t);
    bsp.surfEdges.assign(raw, raw + n);
}

void BSPLoader::ParseModels(BSPFile& bsp, const std::vector<uint8_t>& data,
                             const BSPRawLump& lump) {
    const auto* raw = LumpPtr<BSPRawModel>(data, lump);
    size_t n = LumpCount<BSPRawModel>(lump);
    bsp.models.reserve(n);
    for (size_t i = 0; i < n; i++) {
        BSPModel m;
        m.bounds.mins = { raw[i].mins[0], raw[i].mins[1], raw[i].mins[2] };
        m.bounds.maxs = { raw[i].maxs[0], raw[i].maxs[1], raw[i].maxs[2] };
        m.origin      = { raw[i].origin[0], raw[i].origin[1], raw[i].origin[2] };
        for (int h = 0; h < BSP_MAX_HULLS; h++)
            m.headnode[h] = raw[i].headnode[h];
        m.firstFace = raw[i].firstface;
        m.numFaces  = raw[i].numfaces;
        bsp.models.push_back(m);
    }
}

void BSPLoader::ParseVisibility(BSPFile& bsp, const std::vector<uint8_t>& data,
                                 const BSPRawLump& lump) {
    if (lump.length == 0) return;
    const uint8_t* raw = data.data() + lump.offset;
    bsp.visibility.assign(raw, raw + lump.length);
}

// ─── WAD file list from worldspawn entity ─────────────────────────────────────

void BSPLoader::ExtractWadList(BSPFile& bsp) {
    // Find "wad" key in the first entity (worldspawn)
    const auto& ents = bsp.entities;
    size_t wadPos = ents.find("\"wad\"");
    if (wadPos == std::string::npos) return;

    size_t q1 = ents.find('"', wadPos + 5);
    if (q1 == std::string::npos) return;
    size_t q2 = ents.find('"', q1 + 1);
    if (q2 == std::string::npos) return;

    std::string wadList = ents.substr(q1 + 1, q2 - q1 - 1);

    // Split on semicolons
    std::istringstream ss(wadList);
    std::string token;
    while (std::getline(ss, token, ';')) {
        if (!token.empty())
            bsp.wadFiles.push_back(token);
    }
}

// ─── Build renderable meshes ──────────────────────────────────────────────────

void BSPLoader::BuildMeshes(BSPFile& bsp) {
    bsp.meshes.resize(bsp.rawFaces.size());

    for (size_t fi = 0; fi < bsp.rawFaces.size(); fi++) {
        const BSPRawFace& rf = bsp.rawFaces[fi];
        BSPMesh& mesh = bsp.meshes[fi];

        if (rf.numedges < 3 || rf.texinfo < 0) continue;

        const BSPRawTexInfo& ti = bsp.texInfos[rf.texinfo];
        mesh.textureName = ti.miptex;

        // Texture vectors (s and t)
        Vec3 sVec = { ti.vecs[0][0], ti.vecs[0][1], ti.vecs[0][2] };
        Vec3 tVec = { ti.vecs[1][0], ti.vecs[1][1], ti.vecs[1][2] };
        float sOfs = ti.vecs[0][3];
        float tOfs = ti.vecs[1][3];

        float texW = 1.0f, texH = 1.0f;
        if (ti.miptex >= 0 && ti.miptex < (int)bsp.textures.size()) {
            texW = (float)bsp.textures[ti.miptex].width;
            texH = (float)bsp.textures[ti.miptex].height;
        }
        // Diag: first 3 faces — show tex dims and first vertex UV
        if (fi < 3) {
            std::string tname = (ti.miptex >= 0 && ti.miptex < (int)bsp.textures.size())
                ? bsp.textures[ti.miptex].name : "?";
            fprintf(stderr, "[bsp fi=%zu] tex='%s' W=%.0f H=%.0f numedges=%d\n",
                    fi, tname.c_str(), texW, texH, rf.numedges);
        }

        // Gather face vertices
        std::vector<BSPFaceVertex> fverts;
        fverts.reserve(rf.numedges);

        // Compute lightmap extents for UV
        float mins[2] = { 1e30f, 1e30f };
        float maxs[2] = {-1e30f,-1e30f };

        std::vector<Vec3> positions(rf.numedges);
        std::vector<Vec2> texUVs(rf.numedges);

        for (int e = 0; e < rf.numedges; e++) {
            int32_t se = bsp.surfEdges[rf.firstedge + e];
            uint16_t vi = (se >= 0) ? bsp.edges[se].v[0] : bsp.edges[-se].v[1];
            Vec3 pos = bsp.vertices[vi];
            positions[e] = pos;

            float s = sVec.Dot(pos) + sOfs;
            float t = tVec.Dot(pos) + tOfs;
            texUVs[e] = { s / texW, t / texH };

            mins[0] = std::min(mins[0], s);
            mins[1] = std::min(mins[1], t);
            maxs[0] = std::max(maxs[0], s);
            maxs[1] = std::max(maxs[1], t);
        }

        // Lightmap extents (aligned to 16-unit grid per GoldSrc)
        float lmins[2], lmaxs[2];
        int   lsize[2];
        for (int c = 0; c < 2; c++) {
            lmins[c] = std::floor(mins[c] / 16.0f);
            lmaxs[c] = std::ceil (maxs[c] / 16.0f);
            lsize[c] = (int)(lmaxs[c] - lmins[c]) + 1;
        }
        float lmW = (float)lsize[0];
        float lmH = (float)lsize[1];

        for (int e = 0; e < rf.numedges; e++) {
            BSPFaceVertex fv;
            fv.pos    = positions[e];
            fv.texUV  = texUVs[e];

            float s = sVec.Dot(positions[e]) + sOfs;
            float t = tVec.Dot(positions[e]) + tOfs;
            fv.uv = {
                (s / 16.0f - lmins[0] + 0.5f) / lmW,
                (t / 16.0f - lmins[1] + 0.5f) / lmH
            };
            fverts.push_back(fv);
        }

        // Fan triangulation
        mesh.verts = fverts;
        mesh.indices.reserve((rf.numedges - 2) * 3);
        for (int e = 1; e < rf.numedges - 1; e++) {
            mesh.indices.push_back(0);
            mesh.indices.push_back(e);
            mesh.indices.push_back(e + 1);
        }

        // Lightmap id (encoded as face index for now; renderer assigns GPU textures)
        mesh.lightmapId = (rf.lightofs >= 0) ? (int)fi : -1;
    }
}

// ─── BSPTree ──────────────────────────────────────────────────────────────────

int BSPTree::FindLeaf(Vec3 point) const {
    return TraverseNode(0, point);
}

int BSPTree::FindLeafInModel(Vec3 point, int modelIndex) const {
    if (modelIndex < 0 || modelIndex >= (int)m_bsp.models.size()) return -1;
    int headnode = m_bsp.models[modelIndex].headnode[0];
    return TraverseNode(headnode, point);
}

int BSPTree::TraverseNode(int idx, Vec3 point) const {
    while (idx >= 0) {
        const BSPNode& node = m_bsp.nodes[idx];
        float d = node.plane.DistanceTo(point);
        idx = node.children[d < 0 ? 1 : 0];
    }
    // idx is -(leaf+1), so leaf = -idx-1
    return -idx - 1;
}

int BSPTree::PointContents(Vec3 point, int hull) const {
    if (m_bsp.models.empty()) return CONTENTS_EMPTY;
    return PointContentsInModel(point, 0, hull);
}

int BSPTree::PointContentsInModel(Vec3 point, int modelIndex, int hull) const {
    if (modelIndex < 0 || modelIndex >= (int)m_bsp.models.size())
        return CONTENTS_EMPTY;

    if (hull == 0) {
        // Use leaf contents from BSP node tree
        int leaf = FindLeafInModel(point, modelIndex);
        if (leaf < 0 || leaf >= (int)m_bsp.leaves.size()) return CONTENTS_EMPTY;
        return m_bsp.leaves[leaf].contents;
    }

    // Use clipnode tree for hulls 1-3
    int headnode = m_bsp.models[modelIndex].headnode[hull];
    return TraverseClipNode(headnode, point, hull);
}

int BSPTree::TraverseClipNode(int idx, Vec3 point, int /*hull*/) const {
    while (idx >= 0) {
        if (idx >= (int)m_bsp.clipNodes.size()) return CONTENTS_EMPTY;
        const BSPClipNode& cn = m_bsp.clipNodes[idx];
        float d = cn.plane.DistanceTo(point);
        idx = cn.children[d < 0 ? 1 : 0];
    }
    return idx; // negative = BSPContents value
}

bool BSPTree::IsVisible(int leafA, int leafB) const {
    if (leafA == leafB) return true;
    if (m_bsp.visibility.empty()) return true; // No vis data = everything visible

    if (leafA <= 0 || leafA >= (int)m_bsp.leaves.size()) return false;
    const BSPLeaf& la = m_bsp.leaves[leafA];
    if (la.visofs < 0) return true;

    // Decompress PVS RLE and test bit for leafB-1 (leaf 0 is solid sentinel)
    const uint8_t* vis = m_bsp.visibility.data() + la.visofs;
    int numLeaves = (int)m_bsp.leaves.size();
    int testLeaf = leafB - 1;

    for (int c = 1; c < numLeaves; ) {
        if (*vis == 0) {
            // RLE zero run
            vis++;
            c += (*vis++) * 8;
        } else {
            uint8_t bits = *vis++;
            for (int bit = 0; bit < 8 && c < numLeaves; bit++, c++) {
                if (c == testLeaf) return (bits & (1 << bit)) != 0;
            }
        }
    }
    return false;
}

} // namespace OS
