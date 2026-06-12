#pragma once
// Grenade projectile physics — CS 1.6 throw/bounce/fuse behavior.

#include <vector>
#include <functional>
#include "Weapon.h"
#include "../math/Math.h"
#include "../physics/Movement.h"

namespace OS {

enum class GrenadeType { HE, Flash, Smoke };

struct Grenade {
    GrenadeType type = GrenadeType::HE;
    Vec3   origin;
    Vec3   velocity;
    float  fuseTime   = 3.0f;  // seconds until detonation
    float  age        = 0;
    bool   atRest     = false;
    bool   detonated  = false;
    int    ownerId    = -1;
    int    bounceCount = 0;
};

struct GrenadeExplosion {
    GrenadeType type;
    Vec3  origin;
    int   ownerId;
};

// CS 1.6 grenade constants (community-measured behavior)
struct GrenadeParams {
    static constexpr float GRAVITY_SCALE   = 0.5f;   // grenades fall at half gravity
    static constexpr float MAX_THROW_SPEED = 750.0f;
    static constexpr float BOUNCE_DAMPING  = 0.55f;  // velocity retained per bounce
    static constexpr float REST_SPEED      = 60.0f;  // below this on ground = rest
    static constexpr float HE_FUSE         = 3.0f;
    static constexpr float FLASH_FUSE      = 1.5f;
    static constexpr float HE_RADIUS       = 350.0f;
    static constexpr float HE_MAX_DAMAGE   = 100.0f;
};

class GrenadeSystem {
public:
    // Compute initial throw velocity (CS 1.6 formula):
    //   speed = (90 - viewPitch) * 6, clamped to MAX_THROW_SPEED,
    //   plus a fraction of thrower velocity
    static Vec3 ThrowVelocity(const Angles& viewAngles, Vec3 throwerVelocity);

    // Spawn a grenade
    void Throw(GrenadeType type, Vec3 origin, Vec3 velocity, int ownerId);

    // Simulate all grenades. Detonations are appended to outExplosions.
    void Update(float dt, float gravity, const TraceFn& trace,
                std::vector<GrenadeExplosion>& outExplosions);

    // HE damage at given distance from blast center (linear falloff)
    static float HEDamage(float distance);

    const std::vector<Grenade>& Active() const { return m_grenades; }

private:
    std::vector<Grenade> m_grenades;
};

} // namespace OS
