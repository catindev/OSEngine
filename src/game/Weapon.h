#pragma once
// Weapon definitions — CS 1.6 weapon parameters.
// Values derived from public CS 1.6 documentation and community research.
// No proprietary data files used.

#include <string>
#include "../math/Math.h"

namespace OS {

enum WeaponID {
    WEAPON_NONE = 0,
    // Pistols
    WEAPON_USP, WEAPON_GLOCK, WEAPON_DEAGLE, WEAPON_P228, WEAPON_ELITES, WEAPON_FIVESEVEN,
    // SMGs
    WEAPON_MP5, WEAPON_TMP, WEAPON_P90, WEAPON_MAC10, WEAPON_UMP45,
    // Rifles
    WEAPON_AK47, WEAPON_M4A1, WEAPON_SG552, WEAPON_GALIL, WEAPON_FAMAS, WEAPON_AUG,
    // Snipers
    WEAPON_AWP, WEAPON_SCOUT, WEAPON_SG550, WEAPON_G3SG1,
    // Shotguns
    WEAPON_M3, WEAPON_XM1014,
    // Machine guns
    WEAPON_M249,
    // Special
    WEAPON_KNIFE, WEAPON_C4, WEAPON_FLASHBANG, WEAPON_HEGRENADE, WEAPON_SMOKEGRENADE,
    WEAPON_COUNT
};

enum WeaponSlot {
    SLOT_NONE     = 0,
    SLOT_PRIMARY  = 1,
    SLOT_SECONDARY = 2,
    SLOT_KNIFE    = 3,
    SLOT_GRENADE  = 4,
    SLOT_C4       = 5,
};

enum FireMode { FIRE_SEMI, FIRE_AUTO, FIRE_BURST };

// CS 1.6 recoil parameters
struct RecoilParams {
    float minRecoilPitch  = 0;
    float maxRecoilPitch  = 0;
    float minRecoilYaw    = 0;
    float maxRecoilYaw    = 0;
    // Recovery rate (degrees/second)
    float recoveryRate    = 0;
    // Extra spread per shot while moving
    float moveSpreadAdd   = 0;
};

// CS 1.6 spread (inaccuracy) in degrees
struct SpreadParams {
    float standStill  = 0; // base spread standing still
    float moving      = 0; // spread while walking
    float crouching   = 0; // spread while crouching
    float jumping     = 0; // spread in air
    float burstBonus  = 0; // additional spread for sustained fire
};

struct WeaponDef {
    WeaponID    id          = WEAPON_NONE;
    std::string name;         // e.g. "ak47"
    std::string displayName;  // e.g. "AK-47"
    WeaponSlot  slot        = SLOT_PRIMARY;
    int         cost        = 0;   // CS 1.6 buy price
    int         ammoType    = 0;
    int         maxAmmo     = 0;   // total reserve ammo
    int         clipSize    = 0;   // rounds per magazine
    float       fireRate    = 0;   // rounds per second
    FireMode    fireMode    = FIRE_AUTO;
    float       reloadTime  = 0;   // seconds
    float       drawTime    = 0;   // seconds to raise weapon
    float       damage      = 0;
    float       armorPenetration = 0; // fraction (0..1)
    float       range       = 0;   // max effective range (units)
    float       rangeModifier = 0; // damage falloff (0.5..1.0)
    int         bulletsPerShot = 1; // shotguns
    RecoilParams recoil;
    SpreadParams spread;
    bool         hasSilencer = false;
    bool         hasBurst    = false;
    bool         canZoom     = false;
    float        zoomFOV1    = 40; // first zoom level
    float        zoomFOV2    = 20; // second zoom level (AWP)
    // Player max speed while holding this weapon (CS 1.6 per-weapon speeds)
    float        maxPlayerSpeed = 250.0f;
    // Sound file (game-relative path into user's CS install)
    std::string  fireSound;
    // Model paths
    std::string  modelV;   // v_model (first-person)
    std::string  modelP;   // p_model (third-person)
    std::string  modelW;   // w_model (world)
};

// Weapon database with all CS 1.6 weapons
class WeaponDatabase {
public:
    static void Init();
    static const WeaponDef* Get(WeaponID id);
    static const WeaponDef* GetByName(const std::string& name);
private:
    static WeaponDef s_defs[WEAPON_COUNT];
};

// Instance of a weapon being carried by a player
struct WeaponState {
    WeaponID id       = WEAPON_NONE;
    int      clip     = 0;
    int      reserve  = 0;
    bool     silenced = false;
    bool     zoomed   = false;
    int      zoomLevel = 0; // 0=none, 1=first, 2=second
    float    nextFire  = 0; // timestamp when next shot allowed
    float    nextReload = 0;
    bool     reloading = false;
    // Accumulated recoil
    float    recoilPitch = 0;
    float    recoilYaw   = 0;
    // Burst fire state
    int      burstShotsLeft = 0;
};

} // namespace OS
