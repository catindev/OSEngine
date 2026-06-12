#include "MDL.h"
#include <fstream>
#include <cstring>
#include <cctype>
#include <algorithm>

namespace OS {

int MDLFile::FindSequence(const std::string& substr) const {
    auto lower = [](std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    std::string needle = lower(substr);
    for (size_t i = 0; i < sequences.size(); i++) {
        if (lower(sequences[i].name).find(needle) != std::string::npos)
            return (int)i;
    }
    return -1;
}

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
    ParseSkins    (mdl, data.data(), hdr);
    ParseSequences(mdl, data.data(), hdr);
    ParseBodyParts(mdl, data.data(), hdr);
    ParseHitBoxes (mdl, data.data(), hdr);

    // Keep raw bytes for animation decoding
    mdl.rawData = std::move(data);

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
        for (int d = 0; d < 6; d++) {
            mdl.bones[i].defValue[d] = raw[i].value[d];
            mdl.bones[i].scale[d]    = raw[i].scale[d];
        }
    }
}

void MDLLoader::ParseSkins(MDLFile& mdl, const uint8_t* data, const MDLRawHeader* hdr) {
    mdl.numSkinRef = hdr->numSkinRef;
    int total = hdr->numSkinRef * hdr->numSkinFamilies;
    if (total <= 0) return;
    const int16_t* raw = reinterpret_cast<const int16_t*>(data + hdr->skinIndex);
    mdl.skinTable.assign(raw, raw + total);
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
        mdl.sequences[i].animIndex = raw[i].animIndex;
        mdl.sequences[i].numBlends = raw[i].numBlends;
        mdl.sequences[i].linearMovement = {
            raw[i].linearMovement[0],
            raw[i].linearMovement[1],
            raw[i].linearMovement[2]
        };
    }
}

// ─── Animation decoding ───────────────────────────────────────────────────────
// GoldSrc anim data layout (public format docs):
//   seq.animIndex → array of per-bone anim structs (blend 0):
//     struct anim { uint16 offset[6]; }  // X,Y,Z,XR,YR,ZR channels
//   offset == 0   → channel constant (bone default value)
//   offset != 0   → RLE stream of animvalue relative to that bone's anim struct:
//     union animvalue { struct { uint8 valid, total; } num; int16 value; }
//   A run covers `total` frames; first `valid` frames have explicit values,
//   remaining frames repeat the last explicit value.

#pragma pack(push,1)
struct MDLRawAnim { uint16_t offset[6]; };
union MDLAnimValue {
    struct { uint8_t valid, total; } num;
    int16_t value;
};
#pragma pack(pop)

static int16_t DecodeAnimValue(const MDLRawAnim* panim, int dof, int frame) {
    const auto* pv = reinterpret_cast<const MDLAnimValue*>(
        reinterpret_cast<const uint8_t*>(panim) + panim->offset[dof]);

    int f = frame;
    while (pv->num.total <= f) {
        f -= pv->num.total;
        pv += pv->num.valid + 1;
        if (pv->num.total == 0) return 0; // corrupt stream guard
    }
    if (pv->num.valid > f)
        return pv[f + 1].value;
    return pv[pv->num.valid].value;
}

static void BoneFramePose(const MDLBone& bone, const MDLRawAnim* panim,
                           int frame, Vec3& pos, Quat& rot) {
    float v[6];
    for (int d = 0; d < 6; d++) {
        v[d] = bone.defValue[d];
        if (panim && panim->offset[d] != 0)
            v[d] += DecodeAnimValue(panim, d, frame) * bone.scale[d];
    }
    pos = { v[0], v[1], v[2] };
    rot = Quat::FromEulerXYZ({ v[3], v[4], v[5] });
}

void MDLAnimator::ComputeBones(const MDLFile& mdl, int sequence, float frame,
                                std::vector<Mat4>& out) {
    out.resize(mdl.bones.size());
    if (mdl.bones.empty()) return;

    const MDLRawAnim* anims = nullptr;
    int f0 = 0, f1 = 0;
    float frac = 0;

    if (sequence >= 0 && sequence < (int)mdl.sequences.size() && !mdl.rawData.empty()) {
        const MDLSequence& seq = mdl.sequences[sequence];
        int maxFrame = std::max(0, seq.numFrames - 1);
        f0 = std::min((int)frame, maxFrame);
        f1 = std::min(f0 + 1,     maxFrame);
        frac = frame - (float)f0;
        if (seq.animIndex > 0 &&
            (size_t)seq.animIndex + mdl.bones.size() * sizeof(MDLRawAnim) <= mdl.rawData.size()) {
            anims = reinterpret_cast<const MDLRawAnim*>(mdl.rawData.data() + seq.animIndex);
        }
    }

    std::vector<Mat4> local(mdl.bones.size());
    for (size_t b = 0; b < mdl.bones.size(); b++) {
        const MDLRawAnim* pa = anims ? &anims[b] : nullptr;

        Vec3 p0, p1; Quat q0, q1;
        BoneFramePose(mdl.bones[b], pa, f0, p0, q0);
        if (f1 != f0 && frac > 0.001f) {
            BoneFramePose(mdl.bones[b], pa, f1, p1, q1);
            p0 = Lerp(p0, p1, frac);
            q0 = Quat::Slerp(q0, q1, frac);
        }
        local[b] = Mat4::FromQuatPos(q0, p0);
    }

    // Chain to parents (bones are sorted parent-first in MDL)
    for (size_t b = 0; b < mdl.bones.size(); b++) {
        int parent = mdl.bones[b].parent;
        out[b] = (parent >= 0) ? out[parent] * local[b] : local[b];
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

                // Texture dimensions for UV normalization
                float texW = 1.0f, texH = 1.0f;
                int texIdx = mdl.SkinTexture(mesh.skinRef);
                if (texIdx >= 0 && texIdx < (int)mdl.textures.size()) {
                    texW = std::max(1.0f, (float)mdl.textures[texIdx].width);
                    texH = std::max(1.0f, (float)mdl.textures[texIdx].height);
                }

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
                        v.uv     = { (float)si / texW, (float)ti / texH };
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
