#pragma once
// CVar system — console variables for engine configuration.
// Mirrors CS 1.6 cvar behavior (persistent, range-checked values).

#include <string>
#include <unordered_map>
#include <functional>

namespace OS {

enum CVarFlags {
    CVAR_NONE      = 0,
    CVAR_ARCHIVE   = 1 << 0, // saved to config.cfg
    CVAR_USERINFO  = 1 << 1, // sent to server on change
    CVAR_SERVER    = 1 << 2, // server-side only
    CVAR_READONLY  = 1 << 3, // cannot be changed by user
};

struct CVar {
    std::string name;
    std::string value;
    std::string defaultValue;
    float       fValue  = 0;
    int         iValue  = 0;
    int         flags   = CVAR_NONE;
    float       min     = -1e30f;
    float       max     =  1e30f;
    std::string description;

    using ChangeCallback = std::function<void(const CVar&)>;
    ChangeCallback onChange;

    CVar() = default;
    CVar(const std::string& name, const std::string& def, int flags = CVAR_NONE,
         const std::string& desc = "")
        : name(name), value(def), defaultValue(def), flags(flags), description(desc) {
        Update();
    }

    void SetValue(const std::string& v);
    void SetValue(float v);
    void SetValue(int v);
    void Reset() { SetValue(defaultValue); }

private:
    void Update();
};

class CVarSystem {
public:
    static CVarSystem& Instance();

    // Register a cvar. Returns pointer to the registered cvar.
    CVar* Register(const std::string& name, const std::string& defaultValue,
                   int flags = CVAR_NONE, const std::string& desc = "");

    CVar* Find(const std::string& name);
    const CVar* Find(const std::string& name) const;

    void Set(const std::string& name, const std::string& value);
    void Set(const std::string& name, float value);

    std::string Get(const std::string& name, const std::string& def = "") const;
    float       GetFloat(const std::string& name, float def = 0) const;
    int         GetInt  (const std::string& name, int   def = 0) const;

    // Load/save config files
    bool LoadConfig(const std::string& path);
    bool SaveConfig(const std::string& path) const;

    // Execute a console command string (key value pairs)
    void ExecString(const std::string& commands);

private:
    std::unordered_map<std::string, CVar> m_cvars;
};

// ─── Predefined game cvars ─────────────────────────────────────────────────────

struct GameCVars {
    static void Register(CVarSystem& sys);

    // Movement
    static CVar* sv_gravity;
    static CVar* sv_maxspeed;
    static CVar* sv_accelerate;
    static CVar* sv_airaccelerate;
    static CVar* sv_friction;
    static CVar* sv_stopspeed;
    static CVar* sv_stepsize;
    static CVar* sv_bounce;
    static CVar* sv_maxvelocity;

    // Game
    static CVar* mp_timelimit;
    static CVar* mp_maxrounds;
    static CVar* mp_roundtime;
    static CVar* mp_freezetime;
    static CVar* mp_c4timer;
    static CVar* mp_startmoney;
    static CVar* mp_maxmoney;
    static CVar* mp_friendlyfire;
    static CVar* mp_autokick;
    static CVar* mp_limitteams;
    static CVar* mp_autoteambalance;
    static CVar* mp_buytime;

    // Client
    static CVar* cl_updaterate;
    static CVar* cl_cmdrate;
    static CVar* cl_interp;
    static CVar* cl_fov;
    static CVar* sensitivity;

    // Render
    static CVar* r_drawworld;
    static CVar* r_drawentities;
    static CVar* r_fullbright;
    static CVar* r_wireframe;
};

} // namespace OS
