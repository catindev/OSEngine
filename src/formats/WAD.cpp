#include "WAD.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cctype>

namespace OS {

static std::string ToLower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

static std::string NullTermStr(const char* buf, size_t maxlen) {
    return std::string(buf, strnlen(buf, maxlen));
}

WADFile::WADFile(const std::string& path) : m_path(path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        m_error = "Cannot open WAD: " + path;
        return;
    }

    auto sz = f.tellg(); f.seekg(0);
    m_data.resize(sz);
    f.read(reinterpret_cast<char*>(m_data.data()), sz);

    if (m_data.size() < sizeof(WAD3Header)) {
        m_error = "WAD file too small";
        return;
    }

    const auto* hdr = reinterpret_cast<const WAD3Header*>(m_data.data());
    if (memcmp(hdr->magic, "WAD3", 4) != 0) {
        // Also accept WAD2 (Quake format, limited support)
        if (memcmp(hdr->magic, "WAD2", 4) != 0) {
            m_error = "Not a WAD3 file: " + path;
            return;
        }
    }

    if (hdr->numEntries < 0 || hdr->dirOffset < 0) {
        m_error = "Invalid WAD header";
        return;
    }

    size_t dirEnd = (size_t)hdr->dirOffset + (size_t)hdr->numEntries * sizeof(WAD3DirEntry);
    if (dirEnd > m_data.size()) {
        m_error = "WAD directory extends past end of file";
        return;
    }

    const auto* dir = reinterpret_cast<const WAD3DirEntry*>(m_data.data() + hdr->dirOffset);
    m_entries.assign(dir, dir + hdr->numEntries);

    for (size_t i = 0; i < m_entries.size(); i++) {
        std::string name = NullTermStr(m_entries[i].name, 16);
        m_index[ToLower(name)] = i;
    }

    m_valid = true;
}

const WADTexture* WADFile::GetTexture(const std::string& name) const {
    std::string key = ToLower(name);
    auto cached = m_cache.find(key);
    if (cached != m_cache.end()) return &cached->second;

    auto it = m_index.find(key);
    if (it == m_index.end()) return nullptr;

    WADTexture tex;
    if (!DecodeTexture(m_entries[it->second], tex)) return nullptr;

    m_cache[key] = std::move(tex);
    return &m_cache[key];
}

std::vector<std::string> WADFile::ListTextures() const {
    std::vector<std::string> names;
    names.reserve(m_entries.size());
    for (const auto& e : m_entries)
        names.push_back(NullTermStr(e.name, 16));
    return names;
}

void WADFile::LoadAll() {
    for (const auto& e : m_entries) {
        std::string key = ToLower(NullTermStr(e.name, 16));
        if (m_cache.count(key)) continue;
        WADTexture tex;
        if (DecodeTexture(e, tex))
            m_cache[key] = std::move(tex);
    }
}

bool WADFile::DecodeTexture(const WAD3DirEntry& entry, WADTexture& out) const {
    if (entry.type != WAD_TYPE_MIPTEX) return false;
    if (entry.offset < 0 || entry.diskSize < (int32_t)sizeof(WAD3MipTex)) return false;

    size_t end = (size_t)entry.offset + (size_t)entry.diskSize;
    if (end > m_data.size()) return false;

    const auto* miptex = reinterpret_cast<const WAD3MipTex*>(m_data.data() + entry.offset);
    out.name   = NullTermStr(miptex->name, 16);
    out.width  = miptex->width;
    out.height = miptex->height;

    if (out.width == 0 || out.height == 0) return false;

    uint32_t mip0size = out.width * out.height;
    if (miptex->offsets[0] == 0) return false;

    // Pixel data
    const uint8_t* pixels = reinterpret_cast<const uint8_t*>(miptex) + miptex->offsets[0];
    // After all 4 mip levels: 2 byte padding, then 768 byte palette (256*3)
    uint32_t mip1 = mip0size / 4;
    uint32_t mip2 = mip1 / 4;
    uint32_t mip3 = mip2 / 4;
    const uint8_t* palPtr = pixels + mip0size + mip1 + mip2 + mip3 + 2;

    // Validate palette is within data
    size_t palEnd = (size_t)(palPtr - m_data.data()) + 768;
    if (palEnd > m_data.size()) return false;

    out.pixels.resize(mip0size * 4);
    for (uint32_t p = 0; p < mip0size; p++) {
        uint8_t idx = pixels[p];
        out.pixels[p*4+0] = palPtr[idx*3+0];
        out.pixels[p*4+1] = palPtr[idx*3+1];
        out.pixels[p*4+2] = palPtr[idx*3+2];
        // Color key transparency: index 255 in textures starting with '{'
        out.pixels[p*4+3] = (out.name[0] == '{' && idx == 255) ? 0 : 255;
    }
    return true;
}

// ─── WADManager ───────────────────────────────────────────────────────────────

bool WADManager::AddWAD(const std::string& path) {
    m_wads.emplace_back(path);
    return m_wads.back().IsValid();
}

const WADTexture* WADManager::FindTexture(const std::string& name) const {
    // Search in reverse order (last added = highest priority)
    for (int i = (int)m_wads.size() - 1; i >= 0; i--) {
        const WADTexture* t = m_wads[i].GetTexture(name);
        if (t) return t;
    }
    return nullptr;
}

} // namespace OS
