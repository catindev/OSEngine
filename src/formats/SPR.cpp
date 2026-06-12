#include "SPR.h"
#include <fstream>
#include <cstring>

namespace OS {

SPRFile SPRLoader::Load(const std::string& path) {
    SPRFile spr;

    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) { spr.error = "Cannot open SPR: " + path; return spr; }

    auto sz = f.tellg(); f.seekg(0);
    std::vector<uint8_t> data(sz);
    f.read(reinterpret_cast<char*>(data.data()), sz);

    if (data.size() < sizeof(SPRRawHeader) + 768) {
        spr.error = "SPR file too small"; return spr;
    }

    const auto* hdr = reinterpret_cast<const SPRRawHeader*>(data.data());
    if (hdr->magic != SPR_MAGIC) { spr.error = "Not an SPR file"; return spr; }
    if (hdr->version != SPR_VERSION) {
        spr.error = "Unsupported SPR version " + std::to_string(hdr->version);
        return spr;
    }

    spr.type           = (SPRType)hdr->type;
    spr.texFormat      = (SPRFormat)hdr->texFormat;
    spr.boundingRadius = hdr->boundingRadius;

    // Palette follows header (256*3 = 768 bytes), preceded by 2-byte count
    size_t ofs = sizeof(SPRRawHeader);
    uint16_t palCount; memcpy(&palCount, data.data() + ofs, 2); ofs += 2;
    const uint8_t* palette = data.data() + ofs;
    ofs += palCount * 3;

    spr.frames.resize(hdr->numFrames);
    for (int fi = 0; fi < hdr->numFrames; fi++) {
        if (ofs + sizeof(SPRRawFrame) > data.size()) break;
        const auto* rf = reinterpret_cast<const SPRRawFrame*>(data.data() + ofs);
        ofs += sizeof(SPRRawFrame);

        SPRFrame& frame = spr.frames[fi];
        frame.originX = rf->originX;
        frame.originY = rf->originY;
        frame.width   = rf->width;
        frame.height  = rf->height;

        uint32_t pixCount = rf->width * rf->height;
        if (ofs + pixCount > data.size()) break;
        const uint8_t* pixels = data.data() + ofs;
        ofs += pixCount;

        frame.pixels.resize(pixCount * 4);
        for (uint32_t p = 0; p < pixCount; p++) {
            uint8_t idx = pixels[p];
            frame.pixels[p*4+0] = palette[idx*3+0];
            frame.pixels[p*4+1] = palette[idx*3+1];
            frame.pixels[p*4+2] = palette[idx*3+2];
            // Index 255 is transparent for alpha-test format
            frame.pixels[p*4+3] = (spr.texFormat == SPR_FORMAT_ALPHATEST && idx == 255) ? 0 : 255;
        }
    }

    spr.valid = true;
    return spr;
}

} // namespace OS
