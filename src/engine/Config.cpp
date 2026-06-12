#include "Config.h"
#include "../math/Math.h"
#include <fstream>
#include <sstream>
#include <cstdio>

namespace OS {

void CVar::SetValue(const std::string& v) {
    if (flags & CVAR_READONLY) return;
    value = v;
    Update();
    if (onChange) onChange(*this);
}

void CVar::SetValue(float v) {
    v = Clamp(v, min, max);
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", v);
    value = buf;
    Update();
    if (onChange) onChange(*this);
}

void CVar::SetValue(int v) { SetValue((float)v); }

void CVar::Update() {
    try { fValue = std::stof(value); } catch (...) { fValue = 0; }
    iValue = (int)fValue;
}

CVarSystem& CVarSystem::Instance() {
    static CVarSystem inst;
    return inst;
}

CVar* CVarSystem::Register(const std::string& name, const std::string& def,
                            int flags, const std::string& desc) {
    auto it = m_cvars.find(name);
    if (it != m_cvars.end()) return &it->second;
    m_cvars[name] = CVar(name, def, flags, desc);
    return &m_cvars[name];
}

CVar* CVarSystem::Find(const std::string& name) {
    auto it = m_cvars.find(name);
    return it != m_cvars.end() ? &it->second : nullptr;
}

const CVar* CVarSystem::Find(const std::string& name) const {
    auto it = m_cvars.find(name);
    return it != m_cvars.end() ? &it->second : nullptr;
}

void CVarSystem::Set(const std::string& name, const std::string& value) {
    CVar* cv = Find(name);
    if (cv) cv->SetValue(value);
}

void CVarSystem::Set(const std::string& name, float value) {
    CVar* cv = Find(name);
    if (cv) cv->SetValue(value);
}

std::string CVarSystem::Get(const std::string& name, const std::string& def) const {
    const CVar* cv = Find(name);
    return cv ? cv->value : def;
}

float CVarSystem::GetFloat(const std::string& name, float def) const {
    const CVar* cv = Find(name);
    return cv ? cv->fValue : def;
}

int CVarSystem::GetInt(const std::string& name, int def) const {
    const CVar* cv = Find(name);
    return cv ? cv->iValue : def;
}

bool CVarSystem::LoadConfig(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '/' || line[0] == '#') continue;
        ExecString(line);
    }
    return true;
}

bool CVarSystem::SaveConfig(const std::string& path) const {
    std::ofstream f(path);
    if (!f) return false;
    for (const auto& [name, cv] : m_cvars) {
        if (cv.flags & CVAR_ARCHIVE)
            f << name << " \"" << cv.value << "\"\n";
    }
    return true;
}

void CVarSystem::ExecString(const std::string& cmd) {
    std::istringstream ss(cmd);
    std::string name, value;
    ss >> name;
    if (name.empty()) return;
    std::getline(ss >> std::ws, value);
    if (!value.empty() && value.front() == '"') {
        value = value.substr(1);
        if (!value.empty() && value.back() == '"') value.pop_back();
    }
    Set(name, value);
}

// ─── GameCVars static members ─────────────────────────────────────────────────

CVar* GameCVars::sv_gravity;
CVar* GameCVars::sv_maxspeed;
CVar* GameCVars::sv_accelerate;
CVar* GameCVars::sv_airaccelerate;
CVar* GameCVars::sv_friction;
CVar* GameCVars::sv_stopspeed;
CVar* GameCVars::sv_stepsize;
CVar* GameCVars::sv_bounce;
CVar* GameCVars::sv_maxvelocity;
CVar* GameCVars::mp_timelimit;
CVar* GameCVars::mp_maxrounds;
CVar* GameCVars::mp_roundtime;
CVar* GameCVars::mp_freezetime;
CVar* GameCVars::mp_c4timer;
CVar* GameCVars::mp_startmoney;
CVar* GameCVars::mp_maxmoney;
CVar* GameCVars::mp_friendlyfire;
CVar* GameCVars::mp_autokick;
CVar* GameCVars::mp_limitteams;
CVar* GameCVars::mp_autoteambalance;
CVar* GameCVars::mp_buytime;
CVar* GameCVars::cl_updaterate;
CVar* GameCVars::cl_cmdrate;
CVar* GameCVars::cl_interp;
CVar* GameCVars::cl_fov;
CVar* GameCVars::sensitivity;
CVar* GameCVars::r_drawworld;
CVar* GameCVars::r_drawentities;
CVar* GameCVars::r_fullbright;
CVar* GameCVars::r_wireframe;

void GameCVars::Register(CVarSystem& sys) {
    // CS 1.6 default movement values
    sv_gravity       = sys.Register("sv_gravity",       "800",   CVAR_SERVER, "World gravity");
    sv_maxspeed      = sys.Register("sv_maxspeed",      "320",   CVAR_SERVER, "Max player speed");
    sv_accelerate    = sys.Register("sv_accelerate",    "10",    CVAR_SERVER, "Ground acceleration");
    sv_airaccelerate = sys.Register("sv_airaccelerate", "10",    CVAR_SERVER, "Air acceleration");
    sv_friction      = sys.Register("sv_friction",      "4",     CVAR_SERVER, "Ground friction");
    sv_stopspeed     = sys.Register("sv_stopspeed",     "100",   CVAR_SERVER, "Deceleration threshold");
    sv_stepsize      = sys.Register("sv_stepsize",      "18",    CVAR_SERVER, "Max step height");
    sv_bounce        = sys.Register("sv_bounce",        "1",     CVAR_SERVER, "Bounce coefficient");
    sv_maxvelocity   = sys.Register("sv_maxvelocity",   "2000",  CVAR_SERVER, "Max entity velocity");

    // CS 1.6 game rules defaults
    mp_timelimit      = sys.Register("mp_timelimit",      "0",    CVAR_SERVER|CVAR_ARCHIVE);
    mp_maxrounds      = sys.Register("mp_maxrounds",      "0",    CVAR_SERVER|CVAR_ARCHIVE);
    mp_roundtime      = sys.Register("mp_roundtime",      "2.5",  CVAR_SERVER|CVAR_ARCHIVE);
    mp_freezetime     = sys.Register("mp_freezetime",     "6",    CVAR_SERVER|CVAR_ARCHIVE);
    mp_c4timer        = sys.Register("mp_c4timer",        "35",   CVAR_SERVER|CVAR_ARCHIVE);
    mp_startmoney     = sys.Register("mp_startmoney",     "800",  CVAR_SERVER|CVAR_ARCHIVE);
    mp_maxmoney       = sys.Register("mp_maxmoney",       "16000",CVAR_SERVER|CVAR_ARCHIVE);
    mp_friendlyfire   = sys.Register("mp_friendlyfire",   "0",    CVAR_SERVER|CVAR_ARCHIVE);
    mp_autokick       = sys.Register("mp_autokick",       "1",    CVAR_SERVER|CVAR_ARCHIVE);
    mp_limitteams     = sys.Register("mp_limitteams",     "2",    CVAR_SERVER|CVAR_ARCHIVE);
    mp_autoteambalance = sys.Register("mp_autoteambalance","1",   CVAR_SERVER|CVAR_ARCHIVE);
    mp_buytime        = sys.Register("mp_buytime",        "0.25", CVAR_SERVER|CVAR_ARCHIVE);

    // Network
    cl_updaterate = sys.Register("cl_updaterate", "60",  CVAR_USERINFO|CVAR_ARCHIVE);
    cl_cmdrate    = sys.Register("cl_cmdrate",    "60",  CVAR_USERINFO|CVAR_ARCHIVE);
    cl_interp     = sys.Register("cl_interp",     "0.1", CVAR_USERINFO|CVAR_ARCHIVE);

    // Client
    cl_fov     = sys.Register("fov",         "90",  CVAR_ARCHIVE);
    sensitivity = sys.Register("sensitivity", "3",   CVAR_ARCHIVE);

    // Render
    r_drawworld    = sys.Register("r_drawworld",    "1", CVAR_NONE);
    r_drawentities = sys.Register("r_drawentities", "1", CVAR_NONE);
    r_fullbright   = sys.Register("r_fullbright",   "0", CVAR_NONE);
    r_wireframe    = sys.Register("r_wireframe",    "0", CVAR_NONE);
}

} // namespace OS
