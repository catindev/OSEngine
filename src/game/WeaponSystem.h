#pragma once
// Weapon runtime: firing state machine, spread, recoil, hitscan, damage.
// Behavior modeled on observed CS 1.6 mechanics; no Valve code.

#include <vector>
#include <random>
#include "Weapon.h"
#include "../math/Math.h"
#include "../physics/Movement.h"

namespace OS {

// CS 1.6 hit groups (damage multipliers)
enum HitGroup {
    HITGROUP_GENERIC = 0,
    HITGROUP_HEAD    = 1,
    HITGROUP_CHEST   = 2,
    HITGROUP_STOMACH = 3,
    HITGROUP_LEFTARM = 4,
    HITGROUP_RIGHTARM = 5,
    HITGROUP_LEFTLEG = 6,
    HITGROUP_RIGHTLEG = 7,
};

// Damage multipliers per hit group (CS 1.6)
float HitGroupMultiplier(HitGroup g);

// A shootable target (player) for hitscan
struct ShootTarget {
    int  id = -1;
    AABB body;      // body bounding box (world space)
    AABB head;      // head bounding box (world space)
    bool alive = true;
};

struct BulletHit {
    bool     hitWorld   = false;
    bool     hitTarget  = false;
    int      targetId   = -1;
    HitGroup hitGroup   = HITGROUP_GENERIC;
    Vec3     point;
    Vec3     normal;
    float    damage     = 0;
    float    distance   = 0;
};

struct FireEvent {
    bool fired      = false;
    bool dryFire    = false; // empty clip click
    int  bulletsFired = 0;
    std::vector<BulletHit> hits;
    // View punch to apply to camera (degrees)
    float punchPitch = 0;
    float punchYaw   = 0;
};

// Player stance for spread calculation
struct ShooterContext {
    Vec3   eyePos;
    Angles viewAngles;
    float  velocity2D   = 0;   // horizontal speed
    bool   onGround     = true;
    bool   ducking      = false;
    float  time         = 0;   // current game time (seconds)
};

class WeaponSystem {
public:
    WeaponSystem();

    // Seed RNG (для воспроизводимых тестов)
    void SeedRandom(uint32_t seed);

    // Give a weapon: initializes clip/reserve from definition
    static WeaponState MakeWeapon(WeaponID id);

    // Process trigger held/pressed.
    // Returns FireEvent describing what happened.
    FireEvent TryFire(WeaponState& ws, const ShooterContext& ctx,
                      bool triggerHeld, bool triggerPressed,
                      const TraceFn& trace,
                      const std::vector<ShootTarget>& targets);

    // Start reload if possible
    bool StartReload(WeaponState& ws, float time);

    // Per-frame update: completes reloads, decays recoil
    void Update(WeaponState& ws, float time, float dt);

    // Current spread half-angle (degrees) given context
    float ComputeSpread(const WeaponDef& def, const ShooterContext& ctx,
                        const WeaponState& ws) const;

    // CS 1.6 damage falloff: damage * rangeModifier^(distance/500)
    static float FalloffDamage(float baseDamage, float rangeModifier, float distance);

private:
    std::mt19937 m_rng;

    // Uniform in [-1, 1]
    float Rand11();

    BulletHit TraceBullet(Vec3 start, Vec3 dir, const WeaponDef& def,
                          const TraceFn& trace,
                          const std::vector<ShootTarget>& targets);
};

} // namespace OS
