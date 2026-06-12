#include "MDL.h"
#include <fstream>
#include <cstring>

namespace OS {

MDLFile MDLLoader::Load(const std::string& path) {
    MDLFile mdl;

    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) { mdl.error = "Cannot open MDL: " + path; return mdl; }

    auto sz = f.tellg(); f.seekg(0);
    std::vector<uint8_t> data(sz);
    f.read(reinterpret_cast<char*>(data.data()), sz);

    if (data.size() < sizeof(MDLRawHeader)) {
        mdl.error = "File too small"; return mdl;
    }

    const auto* hdr = reinterpret_cast<const MDLRawHeader*>(data.data());
    if (hdr->magic != MDL_MAGIC) { mdl.error = "Not an MDL file (bad magic)"; return mdl; }
    if (hdr->version != MDL_VERSION) {
        mdl.error = "Unsupported MDL version " + std::to_string(hdr->version);
        return mdl;
    }

    mdl.name = std::string(hdr->name, strnlen(hdr->name, 64));
    mdl.eyePosition = { hdr->eyePosition[0], hdr->eyePosition[1], hdr->eyePosition[2] };
    mdl.bounds = {
        { hdr->bbmins[0], hdr->bbmins[1], hdr->bbmins[2] },
        { hdr->bbmaxs[0], hdr->bbmaxs[1], hdr->bbmaxs[2] }
    };

    ParseBones    (mdl, data.data(), hdr);
    ParseTextures (mdl, data.data(), hdr);
    ParseSequences(mdl, data.data(), hdr);
    ParseBodyParts(mdl, data.data(), hdr);
    ParseHitBoxes (mdl, data.data(), hdr);

    mdl.valid = true;
    return mdl;
}

void MDLLoader::ParseBones(MDLFile& mdl, const uint8_t* data, const MDLRawHeader* hdr) {
    const auto* raw = reinterpret_cast<const MDLRawBone*>(data + hdr->boneIndex);
    mdl.bones.resize(hdr->numBones);
    for (int i = 0; i < hdr->numBones; i++) {
        mdl.bones[i].name   = std::string(raw[i].name, strnlen(raw[i].name, 32));
        mdl.bones[i].parent = raw[i].parent;
        mdl.bones[i].flags  = raw[i].flags;
        mdl.bones[i].posScale[0] = raw[i].scale[0];
        mdl.bones[i].posScale[1] = raw[i].scale[1];
        mdl.bones[i].posScale[2] = raw[i].scale[2];
        mdl.bones[i].rotScale[0] = raw[i].scale[3];
        mdl.bones[i].rotScale[1] = raw[i].scale[4];
        mdl.bones[i].rotScale[2] = raw[i].scale[5];
    }
}

void MDLLoader::ParseTextures(MDLFile& mdl, const uint8_t* data, const MDLRawHeader* hdr) {
    const auto* raw = reinterpret_cast<const MDLRawTexture*>(data + hdr->textureIndex);
    mdl.textures.resize(hdr->numTextures);
    for (int i = 0; i < hdr->numTextures; i++) {
        MDLTexture& tex = mdl.textures[i];
        tex.name   = std::string(raw[i].name, strnlen(raw[i].name, 64));
        tex.flags  = raw[i].flags;
        tex.width  = raw[i].width;
        tex.height = raw[i].height;

        if (raw[i].index <= 0 || tex.width == 0 || tex.height == 0) continue;

        uint32_t pixCount = tex.width * tex.height;
        const uint8_t* pixels  = data + raw[i].index;
        const uint8_t* palette = pixels + pixCount;

        tex.pixels.resize(pixCount * 4);
        for (uint32_t p = 0; p < pixCount; p++) {
            uint8_t idx = pixels[p];
            tex.pixels[p*4+0] = palette[idx*3+0];
            tex.pixels[p*4+1] = palette[idx*3+1];
            tex.pixels[p*4+2] = palette[idx*3+2];
            tex.pixels[p*4+3] = (tex.flags & MDL_TEX_MASKED && idx == 255) ? 0 : 255;
        }
    }
}

void MDLLoader::ParseSequences(MDLFile& mdl, const uint8_t* data, const MDLRawHeader* hdr) {
    const auto* raw = reinterpret_cast<const MDLRawSequence*>(data + hdr->seqIndex);
    mdl.sequences.resize(hdr->numSeq);
    for (int i = 0; i < hdr->numSeq; i++) {
        mdl.sequences[i].name      = std::string(raw[i].label, strnlen(raw[i].label, 32));
        mdl.sequences[i].fps       = raw[i].fps;
        mdl.sequences[i].numFrames = raw[i].numFrames;
        mdl.sequences[i].flags     = raw[i].flags;
        mdl.sequences[i].linearMovement = {
            raw[i].linearMovement[0],
            raw[i].linearMovement[1],
            raw[i].linearMovement[2]
        };
    }
}

void MDLLoader::ParseBodyParts(MDLFile& mdl, const uint8_t* data, const MDLRawHeader* hdr) {
    const auto* rawBP = reinterpret_cast<const MDLRawBodyPart*>(data + hdr->bodyPartIndex);
    mdl.bodyParts.resize(hdr->numBodyParts);

    for (int bpi = 0; bpi < hdr->numBodyParts; bpi++) {
        MDLBodyPart& bp = mdl.bodyParts[bpi];
        bp.name = std::string(rawBP[bpi].name, strnlen(rawBP[bpi].name, 64));
        bp.base = rawBP[bpi].base;

        const auto* rawModel = reinterpret_cast<const MDLRawModel*>(data + rawBP[bpi].modelIndex);
        for (int mi = 0; mi < rawBP[bpi].numModels; mi++) {
            const auto& rm = rawModel[mi];
            const auto* meshData = reinterpret_cast<const MDLRawMesh*>(data + rm.meshIndex);
            const uint8_t* vertBones = data + rm.vertInfoIndex;
            const float*   verts     = reinterpret_cast<const float*>(data + rm.vertIndex);
            const float*   normals   = reinterpret_cast<const float*>(data + rm.normIndex);

            for (int mshi = 0; mshi < rm.numMesh; mshi++) {
                MDLMesh mesh;
                mesh.skinRef = meshData[mshi].skinRef;

                // GoldSrc uses trivial triangle strips/fans encoded as short triplets
                // triIndex points to array of: vertex[s/t], vertex[index], ... ending with 0
                const int16_t* tris = reinterpret_cast<const int16_t*>(data + meshData[mshi].triIndex);

                while (*tris != 0) {
                    int16_t cnt = *tris++;
                    bool fan = (cnt < 0);
                    if (fan) cnt = -cnt;

                    uint32_t base_v = (uint32_t)mesh.verts.size();
                    for (int t = 0; t < cnt; t++) {
                        int16_t si  = tris[0]; // s texture coord
                        int16_t ti  = tris[1]; // t texture coord
                        int16_t vi  = tris[2]; // vertex index
                        int16_t ni  = tris[3]; // normal index
                        tris += 4;

                        MDLVertex v;
                        v.pos    = { verts[vi*3], verts[vi*3+1], verts[vi*3+2] };
                        v.normal = { normals[ni*3], normals[ni*3+1], normals[ni*3+2] };
                        v.uv     = { (float)si, (float)ti }; // divided by texture dims later
                        v.bone   = vertBones[vi];
                        mesh.verts.push_back(v);
                    }

                    // Triangulate strip or fan
                    for (int t = 2; t < cnt; t++) {
                        if (fan) {
                            mesh.indices.push_back(base_v);
                            mesh.indices.push_back(base_v + t - 1);
                            mesh.indices.push_back(base_v + t);
                        } else {
                            if (t & 1) {
                                mesh.indices.push_back(base_v + t - 1);
                                mesh.indices.push_back(base_v + t - 2);
                                mesh.indices.push_back(base_v + t);
                            } else {
                                mesh.indices.push_back(base_v + t - 2);
                                mesh.indices.push_back(base_v + t - 1);
                                mesh.indices.push_back(base_v + t);
                            }
                        }
                    }
                }
                bp.meshes.push_back(std::move(mesh));
            }
        }
    }
}

void MDLLoader::ParseHitBoxes(MDLFile& mdl, const uint8_t* data, const MDLRawHeader* hdr) {
#pragma pack(push,1)
    struct RawHitBox { int32_t bone, group; float mins[3]; float maxs[3]; };
#pragma pack(pop)
    const auto* raw = reinterpret_cast<const RawHitBox*>(data + hdr->hitBoxIndex);
    mdl.hitBoxes.resize(hdr->numHitBoxes);
    for (int i = 0; i < hdr->numHitBoxes; i++) {
        mdl.hitBoxes[i].bone  = raw[i].bone;
        mdl.hitBoxes[i].group = raw[i].group;
        mdl.hitBoxes[i].mins  = { raw[i].mins[0], raw[i].mins[1], raw[i].mins[2] };
        mdl.hitBoxes[i].maxs  = { raw[i].maxs[0], raw[i].maxs[1], raw[i].maxs[2] };
    }
}

} // namespace OS
