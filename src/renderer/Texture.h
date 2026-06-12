#pragma once
#include "Renderer.h"
#include <unordered_map>
#include <string>

namespace OS {

// Manages GPU textures with caching by name
class TextureCache {
public:
    explicit TextureCache(Renderer& renderer) : m_renderer(renderer) {}
    ~TextureCache() { Clear(); }

    TextureHandle Get(const std::string& name) const;
    TextureHandle Upload(const std::string& name, const uint8_t* rgba, int w, int h,
                         bool genMips = true);
    void          Remove(const std::string& name);
    void          Clear();

    TextureHandle GetOrUpload(const std::string& name,
                               const uint8_t* rgba, int w, int h);

private:
    Renderer& m_renderer;
    std::unordered_map<std::string, TextureHandle> m_cache;
};

} // namespace OS
