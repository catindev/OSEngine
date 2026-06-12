#pragma once
// WAD3 archive loader (GoldSrc texture format)
// WAD3 is the texture/sprite archive format used by GoldSrc/Half-Life/CS 1.6.
// Format is publicly documented; no proprietary code used.

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace OS {

// WAD3 type identifiers
enum WADEntryType : uint8_t {
    WAD_TYPE_PALETTE   = 0x40, // 64 = raw palette
    WAD_TYPE_COLORMAP  = 0x41, // 65 = color map
    WAD_TYPE_MIPTEX    = 0x43, // 67 = mip texture (same layout as BSP MipTex)
    WAD_TYPE_RAW       = 0x42, // 66 = raw image
    WAD_TYPE_FONT      = 0x46, // 70 = font
};

#pragma pack(push, 1)

struct WAD3Header {
    char    magic[4];   // "WAD3"
    int32_t numEntries;
    int32_t dirOffset;
};

struct WAD3DirEntry {
    int32_t  offset;
    int32_t  diskSize;
    int32_t  size;       // uncompressed size
    uint8_t  type;       // WADEntryType
    uint8_t  compression; // 0 = none
    int16_t  dummy;
    char     name[16];
};

struct WAD3MipTex {
    char     name[16];
    uint32_t width;
    uint32_t height;
    uint32_t offsets[4]; // offsets to mip levels relative to struct start
};

#pragma pack(pop)

// Decoded texture from WAD
struct WADTexture {
    std::string   name;
    uint32_t      width  = 0;
    uint32_t      height = 0;
    // RGBA8 pixel data (mip 0)
    std::vector<uint8_t> pixels;
};

// Loaded WAD file
class WADFile {
public:
    explicit WADFile(const std::string& path);

    bool IsValid() const { return m_valid; }
    const std::string& Error() const { return m_error; }
    const std::string& Path() const { return m_path; }

    // Retrieve a texture by name (case-insensitive)
    // Returns nullptr if not found
    const WADTexture* GetTexture(const std::string& name) const;

    // List all texture names in this WAD
    std::vector<std::string> ListTextures() const;

    // Eagerly decode all textures
    void LoadAll();

    int TextureCount() const { return (int)m_entries.size(); }

private:
    bool DecodeTexture(const WAD3DirEntry& entry, WADTexture& out) const;

    std::string m_path;
    std::vector<uint8_t> m_data;
    std::vector<WAD3DirEntry> m_entries;
    std::unordered_map<std::string, size_t> m_index; // lower-case name -> entry idx

    // Decoded cache
    mutable std::unordered_map<std::string, WADTexture> m_cache;

    bool m_valid = false;
    std::string m_error;
};

// Manager that searches multiple WAD files
class WADManager {
public:
    // Add a WAD file to the search list
    bool AddWAD(const std::string& path);

    // Find texture by name across all WADs
    const WADTexture* FindTexture(const std::string& name) const;

    int WADCount() const { return (int)m_wads.size(); }
    const WADFile& GetWAD(int i) const { return m_wads[i]; }

private:
    std::vector<WADFile> m_wads;
};

} // namespace OS
