#include "Engine.h"
#include "../formats/Entity.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <cstdio>
#include <chrono>

namespace OS {

Engine::Engine() : m_texCache(m_renderer) {}
Engine::~Engine() { Shutdown(); }

bool Engine::Init(const EngineConfig& cfg) {
    m_cfg = cfg;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    // OpenGL context hints
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (cfg.fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    m_window = SDL_CreateWindow("OpenStrike",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        cfg.width, cfg.height, flags);

    if (!m_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    m_glCtx = SDL_GL_CreateContext(m_window);
    if (!m_glCtx) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_MakeCurrent(m_window, m_glCtx);
    SDL_GL_SetSwapInterval(1); // vsync

    // Load OpenGL functions via glad
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        fprintf(stderr, "gladLoadGLLoader failed\n");
        return false;
    }

    // Init renderer
    if (!m_renderer.Init(cfg.width, cfg.height)) {
        fprintf(stderr, "Renderer init failed\n");
        return false;
    }

    // Init audio (non-fatal if it fails)
    if (!m_audio.Init()) {
        fprintf(stderr, "Audio init failed (continuing without audio)\n");
    }

    // Register cvars
    GameCVars::Register(CVarSystem::Instance());

    // Load config
    CVarSystem::Instance().LoadConfig(cfg.csDir + "/cstrike/openStrike.cfg");

    // Init asset manager
    m_assets.Init(cfg.csDir, cfg.gameDir);

    // Init weapon database
    WeaponDatabase::Init();

    // Init camera
    m_camera.fovY   = cfg.fov;
    m_camera.aspect = (float)cfg.width / (float)cfg.height;

    // Init game rules from cvars
    auto& cv = CVarSystem::Instance();
    m_rules.Init(
        cv.GetFloat("mp_freezetime", 6),
        cv.GetFloat("mp_roundtime",  2.5f) * 60.0f,
        cv.GetFloat("mp_c4timer",    35),
        cv.GetInt  ("mp_startmoney", 800),
        cv.GetInt  ("mp_maxmoney",   16000),
        cv.GetInt  ("mp_maxrounds",  30)
    );

    // Local player setup
    m_localPlayer.id   = 0;
    m_localPlayer.name = "Player";
    m_localPlayer.team = TEAM_CT;
    m_localPlayer.alive = true;
    m_localPlayer.physState.origin = {};

    m_running = true;

    if (!cfg.startMap.empty()) {
        if (!LoadMap(cfg.startMap)) {
            fprintf(stderr, "Failed to load start map: %s\n", cfg.startMap.c_str());
            // Continue without map (show blank screen)
        }
    }

    printf("OpenStrike initialized. Renderer: %s %s\n",
           m_renderer.Info().vendor.c_str(),
           m_renderer.Info().version.c_str());

    return true;
}

bool Engine::LoadMap(const std::string& mapName) {
    printf("Loading map: %s\n", mapName.c_str());

    m_currentBSP = m_assets.LoadBSP(mapName);
    if (!m_currentBSP || !m_currentBSP->valid) {
        fprintf(stderr, "Failed to load BSP: %s\n",
                m_currentBSP ? m_currentBSP->error.c_str() : "not found");
        return false;
    }

    printf("BSP loaded: %zu faces, %zu leaves, %zu textures\n",
           m_currentBSP->rawFaces.size(),
           m_currentBSP->leaves.size(),
           m_currentBSP->textures.size());

    // Load WAD textures for this map
    m_assets.LoadWADsForMap(*m_currentBSP);

    // Upload textures: for WAD-external textures, look them up
    for (auto& tex : const_cast<BSPFile*>(m_currentBSP)->textures) {
        if (!tex.embedded && !tex.wadName.empty()) {
            const WADTexture* wt = m_assets.FindTexture(tex.wadName);
            if (wt && !wt->pixels.empty()) {
                tex.pixels   = wt->pixels;
                tex.width    = wt->width;
                tex.height   = wt->height;
                tex.embedded = true;
            }
        }
    }

    // Upload BSP geometry to GPU
    m_bspGPU = {};
    m_renderer.UploadBSP(*m_currentBSP, m_bspGPU, m_texCache);
    // Make BSP available to RenderWorld
    m_renderer.m_currentBSP = &m_bspGPU;

    // Build collision system
    m_collision = std::make_unique<CollisionSystem>(*m_currentBSP);

    // Build player movement with CS 1.6 defaults
    MoveVars mv;
    mv.gravity       = CVarSystem::Instance().GetFloat("sv_gravity",       800);
    mv.maxspeed      = CVarSystem::Instance().GetFloat("sv_maxspeed",      320);
    mv.accelerate    = CVarSystem::Instance().GetFloat("sv_accelerate",    10);
    mv.airaccelerate = CVarSystem::Instance().GetFloat("sv_airaccelerate", 10);
    mv.friction      = CVarSystem::Instance().GetFloat("sv_friction",      4);
    mv.stopspeed     = CVarSystem::Instance().GetFloat("sv_stopspeed",     100);
    mv.stepsize      = CVarSystem::Instance().GetFloat("sv_stepsize",      18);

    m_playerMove = std::make_unique<PlayerMove>(mv, MakeTraceFn(*m_collision));

    // Find spawn points for local player
    auto entities = EntityParser::Parse(m_currentBSP->entities);
    for (const auto& ent : entities) {
        // CT spawns for testing
        if (ent.ClassName() == "info_player_start" ||
            ent.ClassName() == "info_player_counterterrorist") {
            Vec3 origin = ent.GetVec3("origin");
            m_localPlayer.physState.origin = origin;
            m_camera.origin = origin + Vec3(0, 0, 28); // eye height
            m_camera.angles = ent.GetAngles("angles");
            break;
        }
    }

    printf("Map ready: %s\n", mapName.c_str());
    return true;
}

// ─── Main loop ────────────────────────────────────────────────────────────────

void Engine::Run() {
    using Clock = std::chrono::high_resolution_clock;
    auto prev = Clock::now();

    while (m_running) {
        auto now = Clock::now();
        float dt = std::chrono::duration<float>(now - prev).count();
        prev = now;
        // Cap dt to prevent physics explosion on stalls
        if (dt > 0.1f) dt = 0.1f;

        ProcessEvents();
        Update(dt);
        Render();
    }
}

void Engine::ProcessEvents() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            m_running = false;
            break;

        case SDL_KEYDOWN:
            switch (ev.key.keysym.sym) {
            case SDLK_ESCAPE:
                m_mouseLocked = !m_mouseLocked;
                SDL_SetRelativeMouseMode(m_mouseLocked ? SDL_TRUE : SDL_FALSE);
                break;
            case SDLK_SPACE:
                m_jumpPressed = true;
                break;
            case SDLK_F4:
                m_running = false;
                break;
            }
            break;

        case SDLK_SPACE:
            m_jumpPressed = false;
            break;

        case SDL_KEYUP:
            if (ev.key.keysym.sym == SDLK_SPACE)
                m_jumpPressed = false;
            break;

        case SDL_MOUSEMOTION:
            if (m_mouseLocked) {
                float sens = CVarSystem::Instance().GetFloat("sensitivity", 3.0f);
                m_camera.ApplyMouseDelta((float)ev.motion.xrel,
                                          (float)ev.motion.yrel, sens * 0.022f);
            }
            break;

        case SDL_WINDOWEVENT:
            if (ev.window.event == SDL_WINDOWEVENT_RESIZED) {
                int w = ev.window.data1;
                int h = ev.window.data2;
                m_renderer.Resize(w, h);
                m_camera.aspect = (float)w / (float)h;
            }
            break;
        }
    }
}

void Engine::BuildPlayerInput(const uint8_t* keys, PlayerInput& inp) {
    Vec3 fwd = m_camera.Forward();
    Vec3 right = m_camera.Right();

    // Zero out Z for ground movement
    Vec3 moveF = { fwd.x, fwd.y, 0 };
    Vec3 moveR = { right.x, right.y, 0 };

    float fwdMove = 0, sideMove = 0;
    if (keys[SDL_SCANCODE_W]) fwdMove += 1;
    if (keys[SDL_SCANCODE_S]) fwdMove -= 1;
    if (keys[SDL_SCANCODE_D]) sideMove += 1;
    if (keys[SDL_SCANCODE_A]) sideMove -= 1;

    Vec3 wish = moveF * fwdMove + moveR * sideMove;
    float wishLen = wish.Length();
    if (wishLen > kEpsilon) {
        inp.wishDir   = wish / wishLen;
        inp.wishSpeed = CVarSystem::Instance().GetFloat("sv_maxspeed", 320.0f);
    } else {
        inp.wishDir   = {};
        inp.wishSpeed = 0;
    }

    inp.forwardMove = fwdMove;
    inp.sideMove    = sideMove;
    inp.jumpPressed = m_jumpPressed;
    inp.duckPressed = keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_LSHIFT];
}

void Engine::Update(float dt) {
    const uint8_t* keys = SDL_GetKeyboardState(nullptr);

    if (m_playerMove && m_currentBSP) {
        PlayerInput inp;
        BuildPlayerInput(keys, inp);

        m_playerMove->Move(m_localPlayer.physState, inp, dt);

        // Sync camera to player eye position
        m_camera.origin = m_localPlayer.eyePosition();
        // Preserve camera yaw/pitch (set by mouse look)
        m_localPlayer.physState.angles = m_camera.angles;
    }

    // Update audio listener
    m_audio.SetListener(m_camera.origin,
                        m_camera.Forward(),
                        m_camera.Right(),
                        m_camera.Up());

    m_jumpPressed = false; // consumed
}

void Engine::Render() {
    m_renderer.BeginFrame();

    if (m_currentBSP && m_bspGPU.ready) {
        Mat4 vp = m_camera.ViewProjection();
        m_renderer.RenderWorld(*m_currentBSP, vp, m_camera.origin);
    } else {
        // Show startup message
        m_renderer.Draw2DRect({10, 10}, {300, 30}, {0,0,0,0.7f});
        m_renderer.Draw2DText("OpenStrike - No map loaded", {15, 15}, 14, {1,1,1,1});
    }

    // HUD (minimal for now)
    if (m_localPlayer.alive) {
        // Health bar background
        m_renderer.Draw2DRect({10, (float)(m_cfg.height - 40)}, {200, 24}, {0,0,0,0.5f});
        // Health bar fill
        float healthFrac = m_localPlayer.health / 100.0f;
        m_renderer.Draw2DRect({12, (float)(m_cfg.height - 38)},
                              {196 * healthFrac, 20}, {0.2f, 0.8f, 0.2f, 1.0f});
    }

    SDL_GL_SwapWindow(m_window);
}

void Engine::Shutdown() {
    m_audio.Shutdown();
    m_renderer.Shutdown();
    m_texCache.Clear();
    m_bspGPU = {};

    if (m_glCtx)  { SDL_GL_DeleteContext(m_glCtx);  m_glCtx  = nullptr; }
    if (m_window) { SDL_DestroyWindow(m_window);     m_window = nullptr; }
    SDL_Quit();
}

} // namespace OS
