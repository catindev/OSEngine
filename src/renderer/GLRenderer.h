#pragma once
#include "Renderer.h"
#include "Texture.h"
#include "../formats/BSP.h"
#include <vector>

namespace OS {

// Per-face GPU data
struct GLFace {
    uint32_t vao     = 0;
    uint32_t vbo     = 0;
    uint32_t ebo     = 0;
    uint32_t indexCount = 0;
    TextureHandle texture   = INVALID_TEXTURE;
    TextureHandle lightmap  = INVALID_TEXTURE;
    bool uploaded = false;
};

// Compiled BSP ready for rendering
struct GLBSPData {
    std::vector<GLFace> faces;
    std::vector<TextureHandle> lightmapTextures; // one per face (or atlas later)
    bool ready = false;
};

class GLRenderer : public Renderer {
public:
    GLRenderer();
    ~GLRenderer() override;

    bool Init(int width, int height) override;
    void Shutdown() override;
    void Resize(int width, int height) override;

    void BeginFrame() override;
    void EndFrame() override;

    TextureHandle UploadTexture(const uint8_t* rgba, int w, int h,
                                 bool genMips = true) override;
    void DeleteTexture(TextureHandle h) override;

    void RenderWorld(const BSPFile& bsp,
                     const Mat4& viewProjection,
                     Vec3 eyePos) override;

    void RenderModel(const MDLFile& mdl,
                     const Mat4& modelMatrix,
                     const Mat4& viewProjection) override;

    void Draw2DRect(Vec2 pos, Vec2 size, Vec4 color) override;
    void Draw2DTexture(TextureHandle tex, Vec2 pos, Vec2 size) override;
    void Draw2DText(const std::string& text, Vec2 pos, float size, Vec4 color) override;

    const RendererInfo& Info() const override { return m_info; }
    const RenderStats&  Stats() const override { return m_stats; }

    // Upload BSP geometry to GPU
    void UploadBSP(const BSPFile& bsp, GLBSPData& out,
                   TextureCache& texCache);

private:
    uint32_t CompileShader(const char* vert, const char* frag);
    void InitShaders();
    void Init2D();
    void DrawFrustumCulled(const GLBSPData& bspData,
                            const BSPFile& bsp,
                            const Mat4& vp,
                            Vec3 eyePos);

    uint32_t m_worldShader  = 0;
    uint32_t m_modelShader  = 0;
    uint32_t m_uiShader     = 0;

    // 2D rendering
    uint32_t m_quadVAO = 0;
    uint32_t m_quadVBO = 0;

    // Fallback 1x1 white texture
    TextureHandle m_whiteTexture = INVALID_TEXTURE;

    int m_width  = 0;
    int m_height = 0;

    RendererInfo m_info;
    RenderStats  m_stats;

    // Currently bound BSP data (set by RenderWorld)
    const GLBSPData* m_currentBSP = nullptr;
};

} // namespace OS
