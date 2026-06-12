#pragma once
// GoldSrc MDL model format (version 10, "IDST")
// Format documented from public reverse-engineering efforts; no proprietary code.

#include <cstdint>
#include <string>
#include <vector>
#include "../math/Math.h"

namespace OS {

static constexpr uint32_t MDL_MAGIC   = 0x54534449; // "IDST"
static constexpr int32_t  MDL_VERSION = 10;

// MDL texture flags
enum MDLTextureFlags {
    MDL_TEX_FLATSHADE  = 0x0001,
    MDL_TEX_CHROME     = 0x0002,
    MDL_TEX_FULLBRIGHT = 0x0004,
    MDL_TEX_NOMIPS     = 0x0008,
    MDL_TEX_ALPHA      = 0x0010,
    MDL_TEX_ADDITIVE   = 0x0020,
    MDL_TEX_MASKED     = 0x0040,
};

// Sequence flags
enum MDLSeqFlags {
    MDL_SEQ_LOOPING = 0x0001,
};

// Motion flags
enum MDLMotionFlags {
    MDL_MOTION_X   = 0x0001,
    MDL_MOTION_Y   = 0x0002,
    MDL_MOTION_Z   = 0x0004,
    MDL_MOTION_XR  = 0x0008,
    MDL_MOTION_YR  = 0x0010,
    MDL_MOTION_ZR  = 0x0020,
    MDL_MOTION_LX  = 0x0040,
    MDL_MOTION_LY  = 0x0080,
    MDL_MOTION_LZ  = 0x0100,
    MDL_MOTION_AX  = 0x0200,
    MDL_MOTION_AY  = 0x0400,
    MDL_MOTION_AZ  = 0x0800,
    MDL_MOTION_AXR = 0x1000,
    MDL_MOTION_AYR = 0x2000,
    MDL_MOTION_AZR = 0x4000,
};

#pragma pack(push, 1)

struct MDLRawHeader {
    uint32_t magic;      // IDST
    int32_t  version;    // 10
    char     name[64];
    int32_t  length;

    float   eyePosition[3];
    float   mins[3];
    float   maxs[3];
    float   bbmins[3];  // bounding box
    float   bbmaxs[3];

    int32_t flags;

    int32_t numBones;       int32_t boneIndex;
    int32_t numBoneControllers; int32_t boneControllerIndex;
    int32_t numHitBoxes;    int32_t hitBoxIndex;
    int32_t numSeq;         int32_t seqIndex;
    int32_t numSeqGroups;   int32_t seqGroupIndex;
    int32_t numTextures;    int32_t textureIndex;   int32_t textureDataIndex;
    int32_t numSkinRef;
    int32_t numSkinFamilies; int32_t skinIndex;
    int32_t numBodyParts;   int32_t bodyPartIndex;
    int32_t numAttachments; int32_t attachmentIndex;
    int32_t soundTable;     int32_t soundIndex;
    int32_t soundGroups;    int32_t soundGroupIndex;
    int32_t numTransitions; int32_t transitionIndex;
};

struct MDLRawBone {
    char    name[32];
    int32_t parent;
    int32_t flags;
    int32_t boneController[6];
    float   value[6]; // default DOF values
    float   scale[6]; // scale for compressed bone data
};

struct MDLRawTexture {
    char     name[64];
    int32_t  flags;
    int32_t  width;
    int32_t  height;
    int32_t  index; // offset to pixel data in file
};

struct MDLRawSequence {
    char    label[32];
    float   fps;
    int32_t flags;
    int32_t activity;
    int32_t actWeight;
    int32_t numEvents;   int32_t eventIndex;
    int32_t numFrames;
    int32_t numPivots;   int32_t pivotIndex;
    int32_t motionType;
    int32_t motionBone;
    float   linearMovement[3];
    int32_t automovePositionIndex;
    int32_t automoveAngleIndex;
    float   bbmin[3];
    float   bbmax[3];
    int32_t numBlends;
    int32_t animIndex;
    int32_t blendType[2];
    float   blendStart[2];
    float   blendEnd[2];
    int32_t blendParent;
    int32_t seqGroup;
    int32_t entryNode;
    int32_t exitNode;
    int32_t nodeFlags;
    int32_t nextSeq;
};

struct MDLRawBodyPart {
    char    name[64];
    int32_t numModels;
    int32_t base;
    int32_t modelIndex;
};

struct MDLRawModel {
    char    name[64];
    int32_t type;
    float   boundingRadius;
    int32_t numMesh;      int32_t meshIndex;
    int32_t numVerts;     int32_t vertInfoIndex; int32_t vertIndex;
    int32_t numNormals;   int32_t normInfoIndex; int32_t normIndex;
    int32_t numGroups;    int32_t groupIndex;
};

struct MDLRawMesh {
    int32_t numTris;   int32_t triIndex;
    int32_t skinRef;
    int32_t numNorms;  int32_t normIndex;
};

#pragma pack(pop)

// ─── Parsed representation ────────────────────────────────────────────────────

struct MDLBone {
    std::string name;
    int         parent = -1;
    int         flags  = 0;
    float       defValue[6]; // default DOF values: X,Y,Z,XR,YR,ZR
    float       scale[6];    // animation data scale per DOF
};

struct MDLTexture {
    std::string          name;
    int32_t              flags = 0;
    uint32_t             width = 0;
    uint32_t             height = 0;
    std::vector<uint8_t> pixels; // RGBA8
};

struct MDLVertex {
    Vec3 pos;
    Vec3 normal;
    Vec2 uv;
    int  bone = 0;
};

struct MDLMesh {
    std::vector<MDLVertex> verts;
    std::vector<uint32_t>  indices;
    int skinRef = 0;
};

struct MDLBodyPart {
    std::string           name;
    int                   base = 0;
    std::vector<MDLMesh>  meshes;
};

struct MDLSequence {
    std::string name;
    float       fps       = 0;
    int         numFrames = 0;
    int         flags     = 0;
    Vec3        linearMovement;
    int32_t     animIndex = 0; // offset into raw file data (blend 0)
    int32_t     numBlends = 1;
};

struct MDLHitBox {
    int   bone   = 0;
    int   group  = 0;
    Vec3  mins;
    Vec3  maxs;
};

struct MDLFile {
    std::string name;
    Vec3        eyePosition;
    AABB        bounds;

    std::vector<MDLBone>     bones;
    std::vector<MDLTexture>  textures;
    std::vector<MDLBodyPart> bodyParts;
    std::vector<MDLSequence> sequences;
    std::vector<MDLHitBox>   hitBoxes;

    // Skin table: skinTable[family * numSkinRef + skinRef] = texture index
    std::vector<int16_t> skinTable;
    int                  numSkinRef = 0;

    // Raw file bytes kept for on-demand animation decoding
    std::vector<uint8_t> rawData;

    bool valid = false;
    std::string error;

    // Find sequence index whose label contains substr (case-insensitive); -1 if none
    int FindSequence(const std::string& substr) const;

    // Resolve texture index for a mesh skinRef (family 0)
    int SkinTexture(int skinRef) const {
        if (skinRef >= 0 && skinRef < (int)skinTable.size())
            return skinTable[skinRef];
        return skinRef; // fallback: direct index
    }
};

// Computes model-space bone transforms for a sequence at a (fractional) frame.
class MDLAnimator {
public:
    static void ComputeBones(const MDLFile& mdl, int sequence, float frame,
                             std::vector<Mat4>& out);
};

class MDLLoader {
public:
    static MDLFile Load(const std::string& path);

private:
    static void ParseBones   (MDLFile& mdl, const uint8_t* data, const MDLRawHeader* hdr);
    static void ParseTextures(MDLFile& mdl, const uint8_t* data, const MDLRawHeader* hdr);
    static void ParseBodyParts(MDLFile& mdl, const uint8_t* data, const MDLRawHeader* hdr);
    static void ParseSequences(MDLFile& mdl, const uint8_t* data, const MDLRawHeader* hdr);
    static void ParseHitBoxes(MDLFile& mdl, const uint8_t* data, const MDLRawHeader* hdr);
    static void ParseSkins   (MDLFile& mdl, const uint8_t* data, const MDLRawHeader* hdr);
};

} // namespace OS
