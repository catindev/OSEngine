#pragma once
#include <string>
#include "../math/Math.h"
#include "../physics/Movement.h"

namespace OS {

enum Team { TEAM_UNASSIGNED = 0, TEAM_TERRORIST = 1, TEAM_CT = 2, TEAM_SPECTATOR = 3 };

// CS 1.6 damage types
enum DamageType {
    DMG_GENERIC  = 0,
    DMG_BULLET   = 1 << 1,
    DMG_SLASH    = 1 << 2,
    DMG_BURN     = 1 << 3,
    DMG_FREEZE   = 1 << 4,
    DMG_FALL     = 1 << 5,
    DMG_BLAST    = 1 << 6,
    DMG_GRENADE  = 1 << 7,
};

struct Player {
    // Identity
    int    id       = -1;
    std::string name;
    Team   team     = TEAM_UNASSIGNED;
    bool   alive    = false;
    bool   isBot    = false;

    // Physics state
    PlayerState physState;

    // Health / armor
    float  health       = 100.0f;
    float  armor        = 0.0f;
    bool   hasHelmet    = false;

    // Money (CS 1.6 economy)
    int    money        = 800;

    // Weapons
    int    activeWeapon = -1;  // slot index
    int    primaryWeapon = -1;
    int    secondaryWeapon = -1;
    bool   hasC4       = false;
    bool   hasDefuseKit = false;
    int    flashbangCount = 0;
    int    heGrenadeCount = 0;
    int    smokeGrenadeCount = 0;

    // Per-round stats
    int    killsThisRound  = 0;
    int    deaths          = 0;
    int    kills           = 0;
    int    assists         = 0;

    // State flags
    bool   inBuyZone   = false;
    bool   inBombZone  = false;
    bool   inRescueZone = false;
    bool   isDefusing  = false;
    float  defuseTimer = 0.0f;

    // Feet eye position (calculated each frame)
    Vec3  eyePosition() const {
        float eyeH = physState.ducking ?
                     28.0f * 0.5f :  // approximately
                     28.0f;
        return physState.origin + Vec3(0, 0, eyeH);
    }

    void TakeDamage(float dmg, int type = DMG_GENERIC);
    void Respawn();
};

} // namespace OS
