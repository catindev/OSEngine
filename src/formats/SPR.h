#pragma once
// GoldSrc SPR sprite format loader
// Format documented from public reverse-engineering; no proprietary code.

#include <cstdint>
#include <string>
#include <vector>
#include "../math/Math.h"

namespace OS {

static constexpr uint32_t SPR_MAGIC   = 0x50534449; // "IDSP"
static constexpr int32_t  SPR_VERSION = 2;

enum SPRType {
    SPR_VP_PARALLEL_UPRIGHT = 0,
    SPR_FACING_UPRIGHT       = 1,
    SPR_VP_PARALLEL          = 2,
    SPR_ORIENTED             = 3,
    SPR_VP_PARALLEL_ORIENTED = 4,
};

enum SPRFormat {
    SPR_FORMAT_NORMAL    = 0,
    SPR_FORMAT_ADDITIVE  = 1,
    SPR_FORMAT_INDEXALPHA = 2,
    SPR_FORMAT_ALPHATEST = 3,
};

#pragma pack(push, 1)
struct SPRRawHeader {
    uint32_t magic;
    int32_t  version;
    int32_t  type;
    int32_t  texFormat;
    float    boundingRadius;
    int32_t  maxWidth;
    int32_t  maxHeight;
    int32_t  numFrames;
    float    beamLen;
    int32_t  syncType;
};

struct SPRRawFrame {
    int32_t group;
    int32_t originX;
    int32_t originY;
    int32_t width;
    int32_t height;
    // followed by width*height indexed pixels
};
#pragma pack(pop)

struct SPRFrame {
    int32_t              originX = 0;
    int32_t              originY = 0;
    int32_t              width   = 0;
    int32_t              height  = 0;
    std::vector<uint8_t> pixels; // RGBA8
};

struct SPRFile {
    SPRType    type      = SPR_VP_PARALLEL;
    SPRFormat  texFormat = SPR_FORMAT_NORMAL;
    float      boundingRadius = 0;
    std::vector<SPRFrame>  frames;
    bool valid = false;
    std::string error;
};

class SPRLoader {
public:
    static SPRFile Load(const std::string& path);
};

} // namespace OS
