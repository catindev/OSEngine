#pragma once
// Abstract renderer interface.
// Implementations: GLRenderer (OpenGL), MetalRenderer (macOS Metal).

#include <string>
#include <cstdint>
#include "../math/Math.h"

namespace OS {

struct BSPFile;
struct MDLFile;
struct WADTexture;

// GPU texture handle
using TextureHandle = uint32_t;
static constexpr TextureHandle INVALID_TEXTURE = 0;

// GPU buffer handle
using BufferHandle = uint32_t;

// Renderer capabilities
struct RendererInfo {
    std::string name;
    std::string vendor;
    std::string version;
    int         maxTextureSize = 0;
    bool        supportsMSAA   = false;
};

// Render statistics
struct RenderStats {
    int drawCalls     = 0;
    int trisRendered  = 0;
    int facesRendered = 0;
    int facesculled   = 0;
    float frameMs     = 0;
};

// Render mode
enum class RenderMode {
    Authentic, // matches CS 1.6 look
    Enhanced,  // modern shading, higher res
};

class Renderer {
public:
    virtual ~Renderer() = default;

    // Initialize the renderer (called after window creation)
    virtual bool Init(int width, int height) = 0;
    virtual void Shutdown() = 0;

    // Resize viewport
    virtual void Resize(int width, int height) = 0;

    // Begin/end frame
    virtual void BeginFrame() = 0;
    virtual void EndFrame()   = 0;

    // Texture management
    virtual TextureHandle UploadTexture(const uint8_t* rgba, int w, int h,
                                         bool genMips = true) = 0;
    virtual void          DeleteTexture(TextureHandle h) = 0;

    // Render a BSP world
    // viewProjection: combined VP matrix
    // eyePos:         camera world position (for PVS)
    virtual void RenderWorld(const BSPFile& bsp,
                              const Mat4& viewProjection,
                              Vec3 eyePos) = 0;

    // Render an MDL model at a given transform
    virtual void RenderModel(const MDLFile& mdl,
                              const Mat4& modelMatrix,
                              const Mat4& viewProjection) = 0;

    // HUD / 2D rendering
    virtual void Draw2DRect(Vec2 pos, Vec2 size, Vec4 color) = 0;
    virtual void Draw2DTexture(TextureHandle tex, Vec2 pos, Vec2 size) = 0;
    virtual void Draw2DText(const std::string& text, Vec2 pos, float size, Vec4 color) = 0;

    // Settings
    virtual void SetRenderMode(RenderMode mode) { m_mode = mode; }
    virtual RenderMode GetRenderMode() const     { return m_mode; }

    virtual const RendererInfo& Info() const = 0;
    virtual const RenderStats&  Stats() const = 0;

protected:
    RenderMode m_mode = RenderMode::Authentic;
};

} // namespace OS
