#include "Engine.h"
#include "../formats/Entity.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <cstdio>
#include <chrono>
#include <cmath>
#include <algorithm>

namespace OS {

Engine::Engine() : m_texCache(m_renderer) {}
Engine::~Engine() { Shutdown(); }

bool Engine::Init(const EngineConfig& cfg) {
    m_cfg = cfg;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

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
    SDL_GL_SetSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        fprintf(stderr, "gladLoadGLLoader failed\n");
        return false;
    }

    if (!m_renderer.Init(cfg.width, cfg.height)) {
        fprintf(stderr, "Renderer init failed\n");
        return false;
    }

    if (!m_audio.Init()) {
        fprintf(stderr, "Audio init failed (continuing without audio)\n");
    }

    GameCVars::Register(CVarSystem::Instance());
    CVarSystem::Instance().LoadConfig(cfg.csDir + "/cstrike/openStrike.cfg");

    m_assets.Init(cfg.csDir, cfg.gameDir);
    WeaponDatabase::Init();

    m_camera.fovY   = cfg.fov;
    m_camera.aspect = (float)cfg.width / (float)cfg.height;

    auto& cv = CVarSystem::Instance();
    m_rules.Init(
        cv.GetFloat("mp_freezetime", 6),
        cv.GetFloat("mp_roundtime",  2.5f) * 60.0f,
        cv.GetFloat("mp_c4timer",    35),
        cv.GetInt  ("mp_startmoney", 800),
        cv.GetInt  ("mp_maxmoney",   16000),
        cv.GetInt  ("mp_maxrounds",  30)
    );

    m_hud = std::make_unique<HUD>(m_renderer);
    m_hud->SetScreenSize(cfg.width, cfg.height);

    m_localPlayer.id   = 0;
    m_localPlayer.name = "Player";
    m_localPlayer.team = TEAM_CT;
    m_localPlayer.alive = true;
    m_localPlayer.money = cv.GetInt("mp_startmoney", 800);

    GiveDefaultLoadout();

    m_running = true;

    // Capture the mouse immediately so look + movement work on launch.
    // ESC toggles it back off.
    m_mouseLocked = true;
    SDL_SetRelativeMouseMode(SDL_TRUE);

    if (!cfg.startMap.empty()) {
        if (!LoadMap(cfg.startMap)) {
            fprintf(stderr, "Failed to load start map: %s\n", cfg.startMap.c_str());
        }
    }

    printf("OpenStrike initialized. Renderer: %s %s\n",
           m_renderer.Info().vendor.c_str(),
           m_renderer.Info().version.c_str());

    return true;
}

void Engine::GiveDefaultLoadout() {
    m_weapons[SLOT_KNIFE] = WeaponSystem::MakeWeapon(WEAPON_KNIFE);
    // CT default pistol = USP; T = Glock
    WeaponID pistol = (m_localPlayer.team == TEAM_TERRORIST) ? WEAPON_GLOCK : WEAPON_USP;
    m_weapons[SLOT_SECONDARY] = WeaponSystem::MakeWeapon(pistol);
    m_weapons[SLOT_PRIMARY]   = {}; // none
    m_weapons[SLOT_GRENADE]   = {};
    m_activeSlot = SLOT_SECONDARY;
    SetViewModel(pistol);
}

void Engine::SetViewModel(WeaponID id) {
    const WeaponDef* def = WeaponDatabase::Get(id);
    if (!def || def->modelV.empty()) { m_viewModel = nullptr; return; }

    m_viewModel = m_assets.LoadMDL(def->modelV);
    if (m_viewModel && !m_viewModel->valid) m_viewModel = nullptr;

    if (m_viewModel) {
        int idle = m_viewModel->FindSequence("idle");
        m_vmSequence = idle >= 0 ? idle : 0;
        m_vmFrame    = 0;
        m_vmLoop     = true;
    }
}

void Engine::PlayWeaponSound(const std::string& gamePath) {
    if (gamePath.empty()) return;
    std::string full = m_assets.Resolve(gamePath);
    if (full.empty()) return;
    m_audio.PlaySound(full, m_localPlayer.physState.origin, 1.0f, 1.0f, CHAN_WEAPON);
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

    m_assets.LoadWADsForMap(*m_currentBSP);

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

    m_bspGPU = {};
    m_renderer.UploadBSP(*m_currentBSP, m_bspGPU, m_texCache);
    m_renderer.SetCurrentBSP(&m_bspGPU);

    m_collision  = std::make_unique<CollisionSystem>(*m_currentBSP);

    MoveVars mv;
    auto& cv = CVarSystem::Instance();
    mv.gravity       = cv.GetFloat("sv_gravity",       800);
    mv.maxspeed      = cv.GetFloat("sv_maxspeed",      320);
    mv.accelerate    = cv.GetFloat("sv_accelerate",    10);
    mv.airaccelerate = cv.GetFloat("sv_airaccelerate", 10);
    mv.friction      = cv.GetFloat("sv_friction",      4);
    mv.stopspeed     = cv.GetFloat("sv_stopspeed",     100);
    mv.stepsize      = cv.GetFloat("sv_stepsize",      18);

    m_playerMove = std::make_unique<PlayerMove>(mv, MakeTraceFn(*m_collision));

    auto entities = EntityParser::Parse(m_currentBSP->entities);
    Vec3 spawnOrigin = {0, 0, 128}; // safe default above origin
    Angles spawnAngles = {};
    bool foundSpawn = false;
    // Try CT spawn first, then any player start, then worldspawn origin
    for (int pass = 0; pass < 2 && !foundSpawn; pass++) {
        for (const auto& ent : entities) {
            const std::string& cn = ent.ClassName();
            bool isCT = (cn == "info_player_counterterrorist");
            bool isStart = (cn == "info_player_start");
            if ((pass == 0 && isCT) || (pass == 1 && (isCT || isStart))) {
                Vec3 o = ent.GetVec3("origin");
                // Only use spawn if it's not exactly at origin (likely unset)
                if (o.x != 0 || o.y != 0 || o.z != 0) {
                    spawnOrigin = o;
                    spawnAngles = ent.GetAngles("angles");
                    foundSpawn = true;
                    fprintf(stderr, "[spawn] %s at (%.0f,%.0f,%.0f)\n",
                            cn.c_str(), o.x, o.y, o.z);
                    break;
                }
            }
        }
    }
    // Raise spawn above floor so player doesn't start inside geometry
    spawnOrigin.z += 36.0f;
    m_localPlayer.physState.origin = spawnOrigin;
    m_camera.origin = spawnOrigin + Vec3(0, 0, 28);
    m_camera.angles = spawnAngles;

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
        if (dt > 0.1f) dt = 0.1f;

        ProcessEvents();
        Update(dt);
        Render();
    }
}

void Engine::ProcessEvents() {
    m_firePressedEdge = false;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            m_running = false;
            break;

        case SDL_KEYDOWN:
            if (ev.key.repeat) break;
            switch (ev.key.keysym.sym) {
            case SDLK_ESCAPE:
                m_mouseLocked = !m_mouseLocked;
                SDL_SetRelativeMouseMode(m_mouseLocked ? SDL_TRUE : SDL_FALSE);
                break;
            case SDLK_F4:
                m_running = false;
                break;
            case SDLK_r:
                if (m_weaponSys.StartReload(m_weapons[m_activeSlot], m_gameTime)) {
                    // Reload animation
                    if (m_viewModel) {
                        int seq = m_viewModel->FindSequence("reload");
                        if (seq >= 0) { m_vmSequence = seq; m_vmFrame = 0; m_vmLoop = false; }
                    }
                }
                break;
            case SDLK_1: SwitchWeapon(SLOT_PRIMARY);   break;
            case SDLK_2: SwitchWeapon(SLOT_SECONDARY); break;
            case SDLK_3: SwitchWeapon(SLOT_KNIFE);     break;
            case SDLK_4: SwitchWeapon(SLOT_GRENADE);   break;
            case SDLK_g: {
                // Throw HE grenade (if owned)
                if (m_localPlayer.heGrenadeCount > 0) {
                    m_localPlayer.heGrenadeCount--;
                    Vec3 vel = GrenadeSystem::ThrowVelocity(
                        m_camera.angles, m_localPlayer.physState.velocity);
                    m_grenades.Throw(GrenadeType::HE,
                        m_camera.origin + m_camera.Forward() * 16.0f,
                        vel, m_localPlayer.id);
                }
                break;
            }
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
            if (ev.button.button == SDL_BUTTON_LEFT) {
                m_fireHeld = true;
                m_firePressedEdge = true;
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if (ev.button.button == SDL_BUTTON_LEFT)
                m_fireHeld = false;
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
                m_cfg.width = w; m_cfg.height = h;
                if (m_hud) m_hud->SetScreenSize(w, h);
            }
            break;
        }
    }
}

void Engine::SwitchWeapon(int slot) {
    if (slot < 0 || slot >= 6)            return;
    if (m_weapons[slot].id == WEAPON_NONE) return;
    if (slot == m_activeSlot)             return;

    m_activeSlot = slot;
    SetViewModel(m_weapons[slot].id);

    // Draw animation
    if (m_viewModel) {
        int seq = m_viewModel->FindSequence("draw");
        if (seq < 0) seq = m_viewModel->FindSequence("deploy");
        if (seq >= 0) { m_vmSequence = seq; m_vmFrame = 0; m_vmLoop = false; }
    }
}

void Engine::BuildPlayerInput(const uint8_t* keys, PlayerInput& inp) {
    Vec3 fwd   = m_camera.Forward();
    Vec3 right = m_camera.Right();

    Vec3 moveF = { fwd.x, fwd.y, 0 };
    Vec3 moveR = { right.x, right.y, 0 };

    float fwdMove = 0, sideMove = 0;
    if (keys[SDL_SCANCODE_W]) fwdMove += 1;
    if (keys[SDL_SCANCODE_S]) fwdMove -= 1;
    if (keys[SDL_SCANCODE_D]) sideMove += 1;
    if (keys[SDL_SCANCODE_A]) sideMove -= 1;

    // CS 1.6: max speed is weapon-dependent
    float maxSpeed = CVarSystem::Instance().GetFloat("sv_maxspeed", 320.0f);
    const WeaponDef* def = WeaponDatabase::Get(m_weapons[m_activeSlot].id);
    if (def) maxSpeed = std::min(maxSpeed, def->maxPlayerSpeed);

    Vec3 wish = moveF * fwdMove + moveR * sideMove;
    float wishLen = wish.Length();
    if (wishLen > kEpsilon) {
        inp.wishDir   = wish / wishLen;
        inp.wishSpeed = maxSpeed;
    } else {
        inp.wishDir   = {};
        inp.wishSpeed = 0;
    }

    inp.forwardMove = fwdMove;
    inp.sideMove    = sideMove;
    inp.jumpPressed = keys[SDL_SCANCODE_SPACE];
    inp.duckPressed = keys[SDL_SCANCODE_LCTRL];
    inp.walkPressed = keys[SDL_SCANCODE_LSHIFT];
}

void Engine::UpdateWeapon(float dt) {
    WeaponState& ws = m_weapons[m_activeSlot];
    if (ws.id == WEAPON_NONE) return;

    m_weaponSys.Update(ws, m_gameTime, dt);

    if (!m_collision) return;

    ShooterContext ctx;
    ctx.eyePos     = m_camera.origin;
    ctx.viewAngles = m_camera.angles;
    ctx.viewAngles.pitch -= m_punchPitch; // recoil punch raises aim
    ctx.velocity2D = m_localPlayer.physState.velocity.Length2D();
    ctx.onGround   = m_localPlayer.physState.onGround;
    ctx.ducking    = m_localPlayer.physState.ducking;
    ctx.time       = m_gameTime;

    std::vector<ShootTarget> targets; // bots/players: Phase 3

    TraceFn trace = MakeTraceFn(*m_collision);
    FireEvent ev = m_weaponSys.TryFire(ws, ctx, m_fireHeld, m_firePressedEdge,
                                        trace, targets);

    if (ev.fired) {
        const WeaponDef* def = WeaponDatabase::Get(ws.id);
        if (def) PlayWeaponSound(def->fireSound);

        m_punchPitch += ev.punchPitch;

        // Shoot animation
        if (m_viewModel) {
            int seq = m_viewModel->FindSequence("shoot");
            if (seq < 0) seq = m_viewModel->FindSequence("fire");
            if (seq >= 0) { m_vmSequence = seq; m_vmFrame = 0; m_vmLoop = false; }
        }
    } else if (ev.dryFire) {
        PlayWeaponSound("sound/weapons/dryfire_pistol.wav");
    }
}

void Engine::Update(float dt) {
    m_gameTime += dt;
    const uint8_t* keys = SDL_GetKeyboardState(nullptr);

    if (m_playerMove && m_currentBSP) {
        PlayerInput inp;
        BuildPlayerInput(keys, inp);

        m_playerMove->Move(m_localPlayer.physState, inp, dt);

        m_camera.origin = m_localPlayer.eyePosition();
        m_localPlayer.physState.angles = m_camera.angles;
    }

    UpdateWeapon(dt);

    // Grenade simulation
    if (m_collision) {
        std::vector<GrenadeExplosion> explosions;
        TraceFn trace = MakeTraceFn(*m_collision);
        float gravity = CVarSystem::Instance().GetFloat("sv_gravity", 800);
        m_grenades.Update(dt, gravity, trace, explosions);

        for (const auto& ex : explosions) {
            if (ex.type == GrenadeType::HE) {
                PlayWeaponSound("sound/weapons/hegrenade-1.wav");
                // Self damage
                float dist = Distance(ex.origin, m_localPlayer.physState.origin);
                float dmg = GrenadeSystem::HEDamage(dist);
                if (dmg > 0) m_localPlayer.TakeDamage(dmg, DMG_BLAST);
            } else if (ex.type == GrenadeType::Flash) {
                PlayWeaponSound("sound/weapons/flashbang-2.wav");
            }
        }
    }

    // Recoil punch decay
    if (m_punchPitch > 0) {
        m_punchPitch = std::max(0.0f, m_punchPitch - dt * 20.0f);
    }

    // Viewmodel animation advance
    if (m_viewModel && m_vmSequence >= 0 &&
        m_vmSequence < (int)m_viewModel->sequences.size()) {
        const MDLSequence& seq = m_viewModel->sequences[m_vmSequence];
        m_vmFrame += seq.fps * dt;
        float maxF = (float)std::max(0, seq.numFrames - 1);
        if (m_vmFrame >= maxF) {
            if (m_vmLoop || (seq.flags & MDL_SEQ_LOOPING)) {
                m_vmFrame = maxF > 0 ? std::fmod(m_vmFrame, maxF) : 0;
            } else {
                // Return to idle
                int idle = m_viewModel->FindSequence("idle");
                m_vmSequence = idle >= 0 ? idle : 0;
                m_vmFrame    = 0;
                m_vmLoop     = true;
            }
        }
    }

    // Game rules tick
    std::vector<Player> players { m_localPlayer };
    m_rules.Update(dt, players);
    m_localPlayer = players[0];

    m_audio.SetListener(m_camera.origin,
                        m_camera.Forward(),
                        m_camera.Right(),
                        m_camera.Up());
}

void Engine::RenderViewModel() {
    if (!m_viewModel) return;

    std::vector<Mat4> bones;
    MDLAnimator::ComputeBones(*m_viewModel, m_vmSequence, m_vmFrame, bones);

    // Viewmodel rendered at eye origin with view angles
    // (GoldSrc studio space: X forward, Y left, Z up)
    Vec3 f = m_camera.Forward();
    Vec3 r = m_camera.Right();
    Vec3 u = m_camera.Up();

    Mat4 model;
    model(0,0) = f.x; model(0,1) = -r.x; model(0,2) = u.x; model(0,3) = m_camera.origin.x;
    model(1,0) = f.y; model(1,1) = -r.y; model(1,2) = u.y; model(1,3) = m_camera.origin.y;
    model(2,0) = f.z; model(2,1) = -r.z; model(2,2) = u.z; model(2,3) = m_camera.origin.z;
    model(3,3) = 1.0f;

    // Clear depth so the viewmodel never clips into walls
    m_renderer.ClearDepth();
    m_renderer.RenderMDL(*m_viewModel, bones, model,
                         m_camera.ViewProjection(), m_texCache);
}

void Engine::Render() {
    m_renderer.BeginFrame();

    if (m_currentBSP && m_bspGPU.ready) {
        // Apply recoil punch to view
        Camera punched = m_camera;
        punched.angles.pitch -= m_punchPitch;

        Mat4 vp = punched.ViewProjection();
        m_renderer.RenderWorld(*m_currentBSP, vp, punched.origin);

        RenderViewModel();
    } else {
        m_renderer.Draw2DRect({10, 10}, {300, 30}, {0,0,0,0.7f});
    }

    // HUD
    if (m_hud) {
        // Crosshair gap widens with movement/firing (CS-style)
        float gap = 4.0f;
        gap += m_localPlayer.physState.velocity.Length2D() * 0.02f;
        gap += m_punchPitch * 2.0f;
        m_hud->Draw(m_localPlayer, m_weapons[m_activeSlot],
                    m_rules.State(), gap);
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
