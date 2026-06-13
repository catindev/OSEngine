#pragma once
// OpenStrike engine — main loop, system orchestration.

#include <string>
#include <memory>
#include "Camera.h"
#include "Config.h"
#include "AssetManager.h"
#include "HUD.h"
#include "../renderer/GLRenderer.h"
#include "../physics/Movement.h"
#include "../physics/Collision.h"
#include "../audio/Audio.h"
#include "../game/GameRules.h"
#include "../game/Player.h"
#include "../game/WeaponSystem.h"
#include "../game/Projectile.h"

struct SDL_Window;

namespace OS {

struct EngineConfig {
    std::string csDir;          // CS 1.6 install path
    std::string gameDir = "cstrike";
    std::string startMap;       // map to load on startup (e.g. "de_dust2")
    int         width    = 1280;
    int         height   = 720;
    bool        fullscreen = false;
    float       fov      = 90.0f;
    bool        windowed = true;
};

class Engine {
public:
    Engine();
    ~Engine();

    bool Init(const EngineConfig& cfg);
    void Shutdown();
    void Run();           // main loop (blocks until quit)

    bool LoadMap(const std::string& mapName);

    AssetManager& Assets()     { return m_assets; }
    AudioSystem&  Audio()      { return m_audio; }
    CVarSystem&   CVars()      { return CVarSystem::Instance(); }
    GameRules&    Rules()      { return m_rules; }
    Camera&       Cam()        { return m_camera; }
    GLRenderer&   Renderer()   { return m_renderer; }

private:
    void ProcessEvents();
    void Update(float dt);
    void Render();

    void BuildPlayerInput(const uint8_t* keys, PlayerInput& inp);
    void UpdateWeapon(float dt);
    void RenderViewModel();
    void SwitchWeapon(int slot);
    void GiveDefaultLoadout();
    void SetViewModel(WeaponID id);
    void PlayWeaponSound(const std::string& gamePath);

    EngineConfig  m_cfg;
    SDL_Window*   m_window  = nullptr;
    void*         m_glCtx   = nullptr;

    AssetManager  m_assets;
    AudioSystem   m_audio;
    GLRenderer    m_renderer;
    Camera        m_camera;
    GameRules     m_rules;
    std::unique_ptr<HUD> m_hud;

    // Currently loaded map
    const BSPFile*    m_currentBSP = nullptr;
    GLBSPData         m_bspGPU;
    TextureCache      m_texCache;
    std::unique_ptr<CollisionSystem> m_collision;
    std::unique_ptr<PlayerMove>      m_playerMove;

    // Local player + combat
    Player        m_localPlayer;
    WeaponSystem  m_weaponSys;
    GrenadeSystem m_grenades;
    WeaponState   m_weapons[6];        // by WeaponSlot
    int           m_activeSlot = (int)SLOT_KNIFE;
    float         m_gameTime   = 0;

    // Viewmodel
    const MDLFile* m_viewModel = nullptr;
    int            m_vmSequence = 0;
    float          m_vmFrame    = 0;
    bool           m_vmLoop     = true;

    // View punch (recoil applied to camera, decays)
    float         m_punchPitch = 0;

    bool          m_running     = false;
    bool          m_mouseLocked = false;
    bool          m_noclip     = false;

    // Input edge detection
    bool          m_firePressedEdge = false;
    bool          m_fireHeld        = false;
};

} // namespace OS
