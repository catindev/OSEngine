#include "Weapon.h"
#include <cstring>
#include <algorithm>
#include <cctype>

namespace OS {

WeaponDef WeaponDatabase::s_defs[WEAPON_COUNT] = {};

static void Def(WeaponDef& d, WeaponID id, const char* name, const char* display,
                WeaponSlot slot, int cost, int clipSize, int maxAmmo,
                float fireRate, FireMode fm, float reload, float draw,
                float dmg, float ap, float range, float rangeMod) {
    d.id              = id;
    d.name            = name;
    d.displayName     = display;
    d.slot            = slot;
    d.cost            = cost;
    d.clipSize        = clipSize;
    d.maxAmmo         = maxAmmo;
    d.fireRate        = fireRate;
    d.fireMode        = fm;
    d.reloadTime      = reload;
    d.drawTime        = draw;
    d.damage          = dmg;
    d.armorPenetration = ap;
    d.range           = range;
    d.rangeModifier   = rangeMod;
    d.bulletsPerShot  = 1;
    d.modelV = std::string("models/v_") + name + ".mdl";
    d.modelP = std::string("models/p_") + name + ".mdl";
    d.modelW = std::string("models/w_") + name + ".mdl";
}

void WeaponDatabase::Init() {
    // ─── Pistols ──────────────────────────────────────────────────────────────
    Def(s_defs[WEAPON_USP], WEAPON_USP, "usp", "H&K USP .45 Tactical",
        SLOT_SECONDARY, 500, 12, 100, 3.44f, FIRE_SEMI, 2.7f, 1.0f,
        34, 0.50f, 4096, 0.79f);
    s_defs[WEAPON_USP].spread    = {4.0f, 12.0f, 4.0f, 14.0f};
    s_defs[WEAPON_USP].hasSilencer = true;

    Def(s_defs[WEAPON_GLOCK], WEAPON_GLOCK, "glock18", "Glock 18 Select Fire",
        SLOT_SECONDARY, 400, 20, 120, 4.0f, FIRE_AUTO, 2.2f, 1.0f,
        25, 0.46f, 4096, 0.75f);
    s_defs[WEAPON_GLOCK].spread  = {10.0f, 30.0f, 6.0f, 30.0f};
    s_defs[WEAPON_GLOCK].hasBurst = true;

    Def(s_defs[WEAPON_DEAGLE], WEAPON_DEAGLE, "deagle", "Desert Eagle .50AE",
        SLOT_SECONDARY, 650, 7, 35, 4.21f, FIRE_SEMI, 2.2f, 1.0f,
        54, 0.80f, 4096, 0.81f);
    s_defs[WEAPON_DEAGLE].spread = {1.5f, 7.0f, 1.5f, 14.0f};
    s_defs[WEAPON_DEAGLE].recoil = {4.0f, 6.0f, -2.5f, 2.5f, 8.0f};

    Def(s_defs[WEAPON_P228], WEAPON_P228, "p228", "SIG P228",
        SLOT_SECONDARY, 600, 13, 52, 3.85f, FIRE_SEMI, 2.7f, 1.0f,
        32, 0.60f, 4096, 0.80f);

    Def(s_defs[WEAPON_FIVESEVEN], WEAPON_FIVESEVEN, "fiveseven", "FN Five-SeveN",
        SLOT_SECONDARY, 750, 20, 100, 5.0f, FIRE_SEMI, 2.7f, 1.0f,
        20, 0.885f, 4096, 0.885f);

    // ─── SMGs ─────────────────────────────────────────────────────────────────
    Def(s_defs[WEAPON_MP5], WEAPON_MP5, "mp5navy", "H&K MP5-Navy",
        SLOT_PRIMARY, 1500, 30, 120, 9.09f, FIRE_AUTO, 2.63f, 1.0f,
        26, 0.57f, 4096, 0.84f);
    s_defs[WEAPON_MP5].recoil   = {0.75f, 1.5f, -1.5f, 1.5f, 18.0f};
    s_defs[WEAPON_MP5].spread   = {8.0f, 30.0f, 4.0f, 30.0f};

    Def(s_defs[WEAPON_P90], WEAPON_P90, "p90", "FN P90",
        SLOT_PRIMARY, 2350, 50, 100, 9.09f, FIRE_AUTO, 3.4f, 1.0f,
        21, 0.69f, 4096, 0.85f);

    Def(s_defs[WEAPON_MAC10], WEAPON_MAC10, "mac10", "MAC-10",
        SLOT_PRIMARY, 1400, 30, 100, 11.1f, FIRE_AUTO, 3.15f, 1.0f,
        29, 0.45f, 4096, 0.82f);

    Def(s_defs[WEAPON_UMP45], WEAPON_UMP45, "ump45", "H&K UMP45",
        SLOT_PRIMARY, 1700, 25, 100, 8.0f, FIRE_AUTO, 3.5f, 1.0f,
        30, 0.50f, 4096, 0.82f);

    // ─── Rifles ───────────────────────────────────────────────────────────────
    Def(s_defs[WEAPON_AK47], WEAPON_AK47, "ak47", "AK-47",
        SLOT_PRIMARY, 2500, 30, 90, 10.0f, FIRE_AUTO, 2.44f, 1.0f,
        36, 0.775f, 8192, 0.98f);
    s_defs[WEAPON_AK47].recoil  = {2.25f, 4.5f, -1.5f, 1.5f, 10.0f};
    s_defs[WEAPON_AK47].spread  = {2.0f, 14.0f, 1.0f, 14.0f};

    Def(s_defs[WEAPON_M4A1], WEAPON_M4A1, "m4a1", "Colt M4A1 Carbine",
        SLOT_PRIMARY, 3100, 30, 90, 11.11f, FIRE_AUTO, 3.05f, 1.0f,
        32, 0.70f, 8192, 0.97f);
    s_defs[WEAPON_M4A1].recoil  = {2.0f, 4.0f, -1.5f, 1.5f, 10.0f};
    s_defs[WEAPON_M4A1].spread  = {2.0f, 13.0f, 1.0f, 13.0f};
    s_defs[WEAPON_M4A1].hasSilencer = true;

    Def(s_defs[WEAPON_AUG], WEAPON_AUG, "aug", "AUG A3 Para",
        SLOT_PRIMARY, 3500, 30, 90, 9.09f, FIRE_AUTO, 3.3f, 1.0f,
        32, 0.90f, 8192, 0.96f);
    s_defs[WEAPON_AUG].canZoom = true;
    s_defs[WEAPON_AUG].zoomFOV1 = 45;

    Def(s_defs[WEAPON_GALIL], WEAPON_GALIL, "galil", "IMI Galil",
        SLOT_PRIMARY, 2000, 35, 90, 8.70f, FIRE_AUTO, 2.45f, 1.0f,
        30, 0.625f, 8192, 0.98f);

    Def(s_defs[WEAPON_FAMAS], WEAPON_FAMAS, "famas", "GIAT FAMAS",
        SLOT_PRIMARY, 2250, 25, 90, 10.0f, FIRE_BURST, 3.3f, 1.0f,
        30, 0.70f, 8192, 0.96f);
    s_defs[WEAPON_FAMAS].hasBurst = true;

    // ─── Snipers ──────────────────────────────────────────────────────────────
    Def(s_defs[WEAPON_AWP], WEAPON_AWP, "awp", "AI Arctic Warfare Police",
        SLOT_PRIMARY, 4750, 10, 30, 1.25f, FIRE_SEMI, 2.5f, 1.3f,
        115, 0.995f, 16384, 0.99f);
    s_defs[WEAPON_AWP].canZoom  = true;
    s_defs[WEAPON_AWP].zoomFOV1 = 40;
    s_defs[WEAPON_AWP].zoomFOV2 = 20;
    s_defs[WEAPON_AWP].recoil   = {5.0f, 12.0f, -4.0f, 4.0f, 6.0f};
    s_defs[WEAPON_AWP].spread   = {0.5f, 5.0f, 0.25f, 5.0f};

    Def(s_defs[WEAPON_SCOUT], WEAPON_SCOUT, "scout", "Schmidt Scout",
        SLOT_PRIMARY, 2750, 10, 90, 1.5f, FIRE_SEMI, 2.0f, 1.3f,
        75, 0.979f, 16384, 0.98f);
    s_defs[WEAPON_SCOUT].canZoom = true;
    s_defs[WEAPON_SCOUT].zoomFOV1 = 40;
    s_defs[WEAPON_SCOUT].zoomFOV2 = 20;

    // ─── Shotguns ─────────────────────────────────────────────────────────────
    Def(s_defs[WEAPON_M3], WEAPON_M3, "m3", "Benelli M3 Super90",
        SLOT_PRIMARY, 1700, 8, 32, 0.95f, FIRE_SEMI, 2.8f, 1.0f,
        26, 0.50f, 3000, 0.80f);
    s_defs[WEAPON_M3].bulletsPerShot = 9;
    s_defs[WEAPON_M3].spread = {30.0f, 30.0f, 30.0f, 30.0f};

    Def(s_defs[WEAPON_XM1014], WEAPON_XM1014, "xm1014", "Benelli XM1014",
        SLOT_PRIMARY, 3000, 7, 32, 4.35f, FIRE_AUTO, 2.8f, 1.0f,
        26, 0.50f, 3000, 0.80f);
    s_defs[WEAPON_XM1014].bulletsPerShot = 6;

    // ─── Machine guns ─────────────────────────────────────────────────────────
    Def(s_defs[WEAPON_M249], WEAPON_M249, "m249", "FN M249 Para",
        SLOT_PRIMARY, 5750, 100, 200, 10.0f, FIRE_AUTO, 4.7f, 1.0f,
        32, 0.82f, 8192, 0.97f);

    // ─── Knife ────────────────────────────────────────────────────────────────
    s_defs[WEAPON_KNIFE].id    = WEAPON_KNIFE;
    s_defs[WEAPON_KNIFE].name  = "knife";
    s_defs[WEAPON_KNIFE].displayName = "Knife";
    s_defs[WEAPON_KNIFE].slot  = SLOT_KNIFE;
    s_defs[WEAPON_KNIFE].cost  = 0;
    s_defs[WEAPON_KNIFE].damage = 65; // main attack; back stab = 180
    s_defs[WEAPON_KNIFE].fireRate = 2.0f;

    // ─── Grenades ─────────────────────────────────────────────────────────────
    s_defs[WEAPON_FLASHBANG].id = WEAPON_FLASHBANG;
    s_defs[WEAPON_FLASHBANG].name = "flashbang";
    s_defs[WEAPON_FLASHBANG].displayName = "Flashbang";
    s_defs[WEAPON_FLASHBANG].slot = SLOT_GRENADE;
    s_defs[WEAPON_FLASHBANG].cost = 200;

    s_defs[WEAPON_HEGRENADE].id = WEAPON_HEGRENADE;
    s_defs[WEAPON_HEGRENADE].name = "hegrenade";
    s_defs[WEAPON_HEGRENADE].displayName = "HE Grenade";
    s_defs[WEAPON_HEGRENADE].slot = SLOT_GRENADE;
    s_defs[WEAPON_HEGRENADE].cost = 300;
    s_defs[WEAPON_HEGRENADE].damage = 500; // max, falls off with radius

    s_defs[WEAPON_SMOKEGRENADE].id = WEAPON_SMOKEGRENADE;
    s_defs[WEAPON_SMOKEGRENADE].name = "smokegrenade";
    s_defs[WEAPON_SMOKEGRENADE].displayName = "Smoke Grenade";
    s_defs[WEAPON_SMOKEGRENADE].slot = SLOT_GRENADE;
    s_defs[WEAPON_SMOKEGRENADE].cost = 300;

    // ─── C4 ───────────────────────────────────────────────────────────────────
    s_defs[WEAPON_C4].id = WEAPON_C4;
    s_defs[WEAPON_C4].name = "c4";
    s_defs[WEAPON_C4].displayName = "C4 Explosive";
    s_defs[WEAPON_C4].slot = SLOT_C4;
    s_defs[WEAPON_C4].cost = 0;
}

const WeaponDef* WeaponDatabase::Get(WeaponID id) {
    if (id <= WEAPON_NONE || id >= WEAPON_COUNT) return nullptr;
    return &s_defs[id];
}

const WeaponDef* WeaponDatabase::GetByName(const std::string& name) {
    for (int i = 1; i < WEAPON_COUNT; i++) {
        if (s_defs[i].name == name) return &s_defs[i];
    }
    return nullptr;
}

} // namespace OS
