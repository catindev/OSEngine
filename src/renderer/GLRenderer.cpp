#include "GLRenderer.h"
#include "../formats/MDL.h"
#include <glad/glad.h>
#include <cstring>
#include <cstdio>
#include <string>

namespace OS {

// ─── Shaders ──────────────────────────────────────────────────────────────────

static const char* kWorldVert = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aTexUV;
layout(location=2) in vec2 aLMUV;

uniform mat4 uVP;

out vec2 vTexUV;
out vec2 vLMUV;

void main() {
    gl_Position = uVP * vec4(aPos, 1.0);
    vTexUV = aTexUV;
    vLMUV  = aLMUV;
}
)";

static const char* kWorldFrag = R"(
#version 330 core
in vec2 vTexUV;
in vec2 vLMUV;

uniform sampler2D uTexture;
uniform sampler2D uLightmap;
uniform int  uFullbright;

out vec4 FragColor;

void main() {
    vec4 tex = texture(uTexture, vTexUV);
    if (tex.a < 0.1) discard;

    vec3 lm = (uFullbright == 1) ? vec3(1.0) : texture(uLightmap, vLMUV).rgb;
    // GoldSrc-style gamma (approximate 2.2 -> linear, apply lm, back to gamma)
    vec3 color = tex.rgb * lm * 2.0;
    FragColor = vec4(color, tex.a);
}
)";

static const char* kModelVert = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;

uniform mat4 uMVP;
uniform mat4 uModel;

out vec2 vUV;
out vec3 vNormal;

void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vUV = aUV;
    vNormal = mat3(uModel) * aNormal;
}
)";

static const char* kModelFrag = R"(
#version 330 core
in vec2 vUV;
in vec3 vNormal;

uniform sampler2D uTexture;

out vec4 FragColor;

void main() {
    vec4 color = texture(uTexture, vUV);
    if (color.a < 0.1) discard;
    float light = max(0.4, dot(normalize(vNormal), normalize(vec3(0.5,0.5,1.0))));
    FragColor = vec4(color.rgb * light, color.a);
}
)";

static const char* kUIVert = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;

uniform vec2 uPos;
uniform vec2 uSize;
uniform vec2 uScreen;

out vec2 vUV;

void main() {
    vec2 p = uPos + aPos * uSize;
    vec2 ndc = (p / uScreen) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV = aUV;
}
)";

static const char* kUIFrag = R"(
#version 330 core
in vec2 vUV;

uniform sampler2D uTex;
uniform vec4 uColor;
uniform int  uHasTexture;

out vec4 FragColor;

void main() {
    if (uHasTexture == 1)
        FragColor = texture(uTex, vUV) * uColor;
    else
        FragColor = uColor;
}
)";

// ─── Shader compilation ───────────────────────────────────────────────────────

uint32_t GLRenderer::CompileShader(const char* vert, const char* frag) {
    auto Compile = [](GLenum type, const char* src) -> GLuint {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetShaderInfoLog(s, 512, nullptr, log);
            fprintf(stderr, "Shader compile error: %s\n", log);
        }
        return s;
    };

    GLuint vs = Compile(GL_VERTEX_SHADER, vert);
    GLuint fs = Compile(GL_FRAGMENT_SHADER, frag);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, 512, nullptr, log);
        fprintf(stderr, "Shader link error: %s\n", log);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

void GLRenderer::InitShaders() {
    m_worldShader = CompileShader(kWorldVert, kWorldFrag);
    m_modelShader = CompileShader(kModelVert, kModelFrag);
    m_uiShader    = CompileShader(kUIVert,    kUIFrag);
}

// ─── Init ─────────────────────────────────────────────────────────────────────

GLRenderer::GLRenderer() = default;
GLRenderer::~GLRenderer() { Shutdown(); }

bool GLRenderer::Init(int width, int height) {
    m_width  = width;
    m_height = height;

    // Gather info
    m_info.name    = "OpenGL";
    m_info.vendor  = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    m_info.version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    GLint maxTex; glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
    m_info.maxTextureSize = maxTex;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT); // BSP front face = clockwise in GoldSrc space
    glFrontFace(GL_CW);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    InitShaders();
    Init2D();

    // White fallback texture
    uint8_t white[4] = {255,255,255,255};
    m_whiteTexture = UploadTexture(white, 1, 1, false);

    return true;
}

void GLRenderer::Init2D() {
    float quadVerts[] = {
        // pos    uv
        0,0,   0,0,
        1,0,   1,0,
        1,1,   1,1,
        0,0,   0,0,
        1,1,   1,1,
        0,1,   0,1,
    };

    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glBindVertexArray(0);
}

void GLRenderer::Shutdown() {
    if (m_worldShader) glDeleteProgram(m_worldShader);
    if (m_modelShader) glDeleteProgram(m_modelShader);
    if (m_uiShader)    glDeleteProgram(m_uiShader);
    if (m_quadVAO) { glDeleteVertexArrays(1, &m_quadVAO); glDeleteBuffers(1, &m_quadVBO); }
    if (m_mdlVAO) {
        glDeleteVertexArrays(1, &m_mdlVAO);
        glDeleteBuffers(1, &m_mdlVBO);
        glDeleteBuffers(1, &m_mdlEBO);
    }
    m_worldShader = m_modelShader = m_uiShader = 0;
    m_quadVAO = m_quadVBO = 0;
    m_mdlVAO = m_mdlVBO = m_mdlEBO = 0;
    m_mdlVBOSize = m_mdlEBOSize = 0;
}

void GLRenderer::Resize(int width, int height) {
    m_width  = width;
    m_height = height;
    glViewport(0, 0, width, height);
}

// ─── Frame ────────────────────────────────────────────────────────────────────

void GLRenderer::BeginFrame() {
    m_stats = {};
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLRenderer::EndFrame() {
    // SDL_GL_SwapWindow is called by the engine
}

// ─── Textures ─────────────────────────────────────────────────────────────────

TextureHandle GLRenderer::UploadTexture(const uint8_t* rgba, int w, int h, bool genMips) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    if (genMips) {
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    return (TextureHandle)tex;
}

void GLRenderer::DeleteTexture(TextureHandle h) {
    if (h == INVALID_TEXTURE) return;
    GLuint tex = (GLuint)h;
    glDeleteTextures(1, &tex);
}

// ─── BSP upload ───────────────────────────────────────────────────────────────

void GLRenderer::UploadBSP(const BSPFile& bsp, GLBSPData& out,
                             TextureCache& texCache) {
    out.faces.resize(bsp.meshes.size());

    for (size_t fi = 0; fi < bsp.meshes.size(); fi++) {
        const BSPMesh& mesh = bsp.meshes[fi];
        GLFace& gf = out.faces[fi];

        if (mesh.verts.empty() || mesh.indices.empty()) continue;

        // Upload texture
        if (mesh.textureName >= 0 && mesh.textureName < (int)bsp.textures.size()) {
            const BSPTexture& tex = bsp.textures[mesh.textureName];
            if (!tex.name.empty()) {
                gf.texture = texCache.Get(tex.name);
                if (gf.texture == INVALID_TEXTURE && tex.embedded && !tex.pixels.empty()) {
                    gf.texture = texCache.Upload(tex.name, tex.pixels.data(),
                                                  tex.width, tex.height, true);
                }
            }
        }
        if (gf.texture == INVALID_TEXTURE) gf.texture = m_whiteTexture;

        // Upload lightmap (simple: one 1x1 white if no lightmap)
        const BSPRawFace& rf = bsp.rawFaces[fi];
        if (rf.lightofs >= 0 && (size_t)rf.lightofs < bsp.lighting.size()) {
            // Compute lightmap size for this face
            const BSPRawTexInfo& ti = bsp.texInfos[rf.texinfo];
            Vec3 sVec = { ti.vecs[0][0], ti.vecs[0][1], ti.vecs[0][2] };
            Vec3 tVec = { ti.vecs[1][0], ti.vecs[1][1], ti.vecs[1][2] };
            float sOfs = ti.vecs[0][3], tOfs = ti.vecs[1][3];

            float mins[2]={1e30f,1e30f}, maxs[2]={-1e30f,-1e30f};
            for (int e = 0; e < rf.numedges; e++) {
                int32_t se = bsp.surfEdges[rf.firstedge + e];
                uint16_t vi = (se>=0) ? bsp.edges[se].v[0] : bsp.edges[-se].v[1];
                Vec3 pos = bsp.vertices[vi];
                float s = sVec.Dot(pos)+sOfs, t = tVec.Dot(pos)+tOfs;
                mins[0]=std::min(mins[0],s); mins[1]=std::min(mins[1],t);
                maxs[0]=std::max(maxs[0],s); maxs[1]=std::max(maxs[1],t);
            }
            int lw = (int)(std::floor(maxs[0]/16) - std::floor(mins[0]/16)) + 2;
            int lh = (int)(std::floor(maxs[1]/16) - std::floor(mins[1]/16)) + 2;
            lw = std::max(1, std::min(lw, 18));
            lh = std::max(1, std::min(lh, 18));

            // Convert RGB lightmap to RGBA
            const uint8_t* src = bsp.lighting.data() + rf.lightofs;
            size_t needed = (size_t)lw * lh * 3;
            if ((size_t)rf.lightofs + needed <= bsp.lighting.size()) {
                std::vector<uint8_t> lmRGBA(lw * lh * 4);
                for (int p = 0; p < lw*lh; p++) {
                    // GoldSrc lightmap gamma expansion (approximate)
                    lmRGBA[p*4+0] = src[p*3+0];
                    lmRGBA[p*4+1] = src[p*3+1];
                    lmRGBA[p*4+2] = src[p*3+2];
                    lmRGBA[p*4+3] = 255;
                }
                gf.lightmap = UploadTexture(lmRGBA.data(), lw, lh, false);
                glBindTexture(GL_TEXTURE_2D, gf.lightmap);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }
        if (gf.lightmap == INVALID_TEXTURE) gf.lightmap = m_whiteTexture;

        // Build vertex buffer
        struct GLVert { float x,y,z, s,t, ls,lt; };
        std::vector<GLVert> verts(mesh.verts.size());
        for (size_t v = 0; v < mesh.verts.size(); v++) {
            verts[v] = {
                mesh.verts[v].pos.x, mesh.verts[v].pos.y, mesh.verts[v].pos.z,
                mesh.verts[v].texUV.x, mesh.verts[v].texUV.y,
                mesh.verts[v].uv.x, mesh.verts[v].uv.y
            };
        }

        glGenVertexArrays(1, &gf.vao);
        glGenBuffers(1, &gf.vbo);
        glGenBuffers(1, &gf.ebo);

        glBindVertexArray(gf.vao);

        glBindBuffer(GL_ARRAY_BUFFER, gf.vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(GLVert),
                     verts.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gf.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     mesh.indices.size()*sizeof(uint32_t),
                     mesh.indices.data(), GL_STATIC_DRAW);

        // pos
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLVert), (void*)0);
        // texUV
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLVert),
                              (void*)(3*sizeof(float)));
        // lmUV
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GLVert),
                              (void*)(5*sizeof(float)));

        glBindVertexArray(0);

        gf.indexCount = (uint32_t)mesh.indices.size();
        gf.uploaded   = true;
    }

    out.ready = true;
}

// ─── Render world ─────────────────────────────────────────────────────────────

void GLRenderer::RenderWorld(const BSPFile& bsp,
                              const Mat4& vp, Vec3 eyePos) {
    // This method just uses whatever GLBSPData was last set via m_currentBSP.
    // Engine calls UploadBSP before calling RenderWorld.
    if (!m_currentBSP || !m_currentBSP->ready) return;

    bool fullbright = false; // set by cvar in engine

    glUseProgram(m_worldShader);

    GLint vpLoc    = glGetUniformLocation(m_worldShader, "uVP");
    GLint texLoc   = glGetUniformLocation(m_worldShader, "uTexture");
    GLint lmLoc    = glGetUniformLocation(m_worldShader, "uLightmap");
    GLint fbLoc    = glGetUniformLocation(m_worldShader, "uFullbright");

    glUniformMatrix4fv(vpLoc, 1, GL_FALSE, vp.Data());
    glUniform1i(texLoc, 0);
    glUniform1i(lmLoc,  1);
    glUniform1i(fbLoc,  fullbright ? 1 : 0);

    glEnable(GL_DEPTH_TEST);

    for (const GLFace& gf : m_currentBSP->faces) {
        if (!gf.uploaded || gf.indexCount == 0) continue;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gf.texture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gf.lightmap);

        glBindVertexArray(gf.vao);
        glDrawElements(GL_TRIANGLES, gf.indexCount, GL_UNSIGNED_INT, nullptr);

        m_stats.drawCalls++;
        m_stats.trisRendered += gf.indexCount / 3;
        m_stats.facesRendered++;
    }

    glBindVertexArray(0);
}

void GLRenderer::RenderModel(const MDLFile& mdl,
                              const Mat4& modelMatrix,
                              const Mat4& viewProjection) {
    // Static pose render (no external bone setup) — used for w_ models
    std::vector<Mat4> bones;
    MDLAnimator::ComputeBones(mdl, -1, 0, bones);
    // Without a texture cache we cannot upload textures here; callers should
    // prefer RenderMDL. This path renders nothing if textures are absent.
    (void)modelMatrix; (void)viewProjection;
}

void GLRenderer::ClearDepth() {
    glClear(GL_DEPTH_BUFFER_BIT);
}

void GLRenderer::RenderMDL(const MDLFile& mdl,
                            const std::vector<Mat4>& bones,
                            const Mat4& modelMatrix,
                            const Mat4& viewProjection,
                            TextureCache& texCache) {
    if (bones.empty() || mdl.bodyParts.empty()) return;

    // Lazy-create dynamic buffers
    if (m_mdlVAO == 0) {
        glGenVertexArrays(1, &m_mdlVAO);
        glGenBuffers(1, &m_mdlVBO);
        glGenBuffers(1, &m_mdlEBO);

        glBindVertexArray(m_mdlVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_mdlVBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_mdlEBO);
        // pos(3) normal(3) uv(2)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float),
                              (void*)(3*sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float),
                              (void*)(6*sizeof(float)));
        glBindVertexArray(0);
    }

    Mat4 mvp = viewProjection * modelMatrix;

    glUseProgram(m_modelShader);
    glUniformMatrix4fv(glGetUniformLocation(m_modelShader, "uMVP"),
                       1, GL_FALSE, mvp.Data());
    glUniformMatrix4fv(glGetUniformLocation(m_modelShader, "uModel"),
                       1, GL_FALSE, modelMatrix.Data());
    glUniform1i(glGetUniformLocation(m_modelShader, "uTexture"), 0);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE); // MDL winding varies; render both sides

    std::vector<float> skinned;

    for (const MDLBodyPart& bp : mdl.bodyParts) {
        for (const MDLMesh& mesh : bp.meshes) {
            if (mesh.verts.empty() || mesh.indices.empty()) continue;

            // Resolve texture
            int texIdx = mdl.SkinTexture(mesh.skinRef);
            TextureHandle tex = m_whiteTexture;
            if (texIdx >= 0 && texIdx < (int)mdl.textures.size()) {
                const MDLTexture& mt = mdl.textures[texIdx];
                if (!mt.pixels.empty()) {
                    std::string key = mdl.name + "#" + std::to_string(texIdx);
                    tex = texCache.GetOrUpload(key, mt.pixels.data(),
                                                mt.width, mt.height);
                }
            }

            bool chrome = false;
            if (texIdx >= 0 && texIdx < (int)mdl.textures.size())
                chrome = (mdl.textures[texIdx].flags & MDL_TEX_CHROME) != 0;

            // CPU skinning
            skinned.clear();
            skinned.reserve(mesh.verts.size() * 8);
            for (const MDLVertex& v : mesh.verts) {
                int b = (v.bone >= 0 && v.bone < (int)bones.size()) ? v.bone : 0;
                Vec3 p = bones[b].TransformPoint(v.pos);
                Vec3 n = bones[b].TransformDir(v.normal);

                float u = v.uv.x, tv = v.uv.y;
                if (chrome) {
                    // Spherical environment approximation
                    u  = n.x * 0.5f + 0.5f;
                    tv = n.y * 0.5f + 0.5f;
                }

                skinned.insert(skinned.end(),
                    { p.x, p.y, p.z, n.x, n.y, n.z, u, tv });
            }

            glBindVertexArray(m_mdlVAO);

            glBindBuffer(GL_ARRAY_BUFFER, m_mdlVBO);
            size_t vbytes = skinned.size() * sizeof(float);
            if (vbytes > m_mdlVBOSize) {
                glBufferData(GL_ARRAY_BUFFER, vbytes, skinned.data(), GL_DYNAMIC_DRAW);
                m_mdlVBOSize = vbytes;
            } else {
                glBufferData(GL_ARRAY_BUFFER, m_mdlVBOSize, nullptr, GL_DYNAMIC_DRAW);
                glBufferData(GL_ARRAY_BUFFER, vbytes, skinned.data(), GL_DYNAMIC_DRAW);
            }

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_mdlEBO);
            size_t ibytes = mesh.indices.size() * sizeof(uint32_t);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, ibytes, mesh.indices.data(),
                         GL_DYNAMIC_DRAW);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);

            glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indices.size(),
                           GL_UNSIGNED_INT, nullptr);

            m_stats.drawCalls++;
            m_stats.trisRendered += (int)mesh.indices.size() / 3;
        }
    }

    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
}

// ─── 2D ───────────────────────────────────────────────────────────────────────

void GLRenderer::Draw2DRect(Vec2 pos, Vec2 size, Vec4 color) {
    glDisable(GL_DEPTH_TEST);
    glUseProgram(m_uiShader);

    glUniform2f(glGetUniformLocation(m_uiShader, "uPos"), pos.x, pos.y);
    glUniform2f(glGetUniformLocation(m_uiShader, "uSize"), size.x, size.y);
    glUniform2f(glGetUniformLocation(m_uiShader, "uScreen"),
                (float)m_width, (float)m_height);
    glUniform4f(glGetUniformLocation(m_uiShader, "uColor"),
                color.x, color.y, color.z, color.w);
    glUniform1i(glGetUniformLocation(m_uiShader, "uHasTexture"), 0);

    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

void GLRenderer::Draw2DTexture(TextureHandle tex, Vec2 pos, Vec2 size) {
    glDisable(GL_DEPTH_TEST);
    glUseProgram(m_uiShader);

    glUniform2f(glGetUniformLocation(m_uiShader, "uPos"), pos.x, pos.y);
    glUniform2f(glGetUniformLocation(m_uiShader, "uSize"), size.x, size.y);
    glUniform2f(glGetUniformLocation(m_uiShader, "uScreen"),
                (float)m_width, (float)m_height);
    glUniform4f(glGetUniformLocation(m_uiShader, "uColor"), 1,1,1,1);
    glUniform1i(glGetUniformLocation(m_uiShader, "uHasTexture"), 1);
    glUniform1i(glGetUniformLocation(m_uiShader, "uTex"), 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

void GLRenderer::Draw2DText(const std::string& /*text*/, Vec2 /*pos*/,
                              float /*size*/, Vec4 /*color*/) {
    // Font rendering: TODO in Phase 2 (bitmap font from CS 1.6 WAD)
}

} // namespace OS
