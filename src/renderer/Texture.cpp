#include "Texture.h"

namespace OS {

TextureHandle TextureCache::Get(const std::string& name) const {
    auto it = m_cache.find(name);
    return it != m_cache.end() ? it->second : INVALID_TEXTURE;
}

TextureHandle TextureCache::Upload(const std::string& name,
                                    const uint8_t* rgba, int w, int h, bool genMips) {
    auto existing = Get(name);
    if (existing != INVALID_TEXTURE) return existing;
    TextureHandle h2 = m_renderer.UploadTexture(rgba, w, h, genMips);
    m_cache[name] = h2;
    return h2;
}

TextureHandle TextureCache::GetOrUpload(const std::string& name,
                                         const uint8_t* rgba, int w, int h) {
    auto existing = Get(name);
    if (existing != INVALID_TEXTURE) return existing;
    return Upload(name, rgba, w, h, true);
}

void TextureCache::Remove(const std::string& name) {
    auto it = m_cache.find(name);
    if (it == m_cache.end()) return;
    m_renderer.DeleteTexture(it->second);
    m_cache.erase(it);
}

void TextureCache::Clear() {
    for (auto& [name, h] : m_cache)
        m_renderer.DeleteTexture(h);
    m_cache.clear();
}

} // namespace OS
