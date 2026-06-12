#include "Projectile.h"
#include <cmath>
#include <algorithm>

namespace OS {

Vec3 GrenadeSystem::ThrowVelocity(const Angles& viewAngles, Vec3 throwerVelocity) {
    float speed = (90.0f - viewAngles.pitch) * 6.0f;
    speed = Clamp(speed, 0.0f, GrenadeParams::MAX_THROW_SPEED);

    Vec3 forward = viewAngles.Forward();
    // GoldSrc adds thrower velocity to the throw
    return forward * speed + throwerVelocity;
}

void GrenadeSystem::Throw(GrenadeType type, Vec3 origin, Vec3 velocity, int ownerId) {
    Grenade g;
    g.type     = type;
    g.origin   = origin;
    g.velocity = velocity;
    g.ownerId  = ownerId;
    switch (type) {
    case GrenadeType::HE:    g.fuseTime = GrenadeParams::HE_FUSE;    break;
    case GrenadeType::Flash: g.fuseTime = GrenadeParams::FLASH_FUSE; break;
    case GrenadeType::Smoke: g.fuseTime = 1e9f; break; // detonates at rest
    }
    m_grenades.push_back(g);
}

void GrenadeSystem::Update(float dt, float gravity, const TraceFn& trace,
                            std::vector<GrenadeExplosion>& outExplosions) {
    for (auto& g : m_grenades) {
        if (g.detonated) continue;

        g.age += dt;

        // Fuse check (smoke detonates when at rest instead)
        bool shouldDetonate = false;
        if (g.type == GrenadeType::Smoke) {
            shouldDetonate = g.atRest;
        } else {
            shouldDetonate = g.age >= g.fuseTime;
        }
        if (shouldDetonate) {
            g.detonated = true;
            outExplosions.push_back({ g.type, g.origin, g.ownerId });
            continue;
        }

        if (g.atRest) continue;

        // Integrate with half-gravity (grenade gravity scale)
        float grav = gravity * GrenadeParams::GRAVITY_SCALE;
        g.velocity.z -= grav * dt;

        Vec3 end = g.origin + g.velocity * dt;
        TraceResult tr = trace(g.origin, end, Vec3(-2,-2,-2), Vec3(2,2,2), MASK_SOLID);

        if (!tr.hit) {
            g.origin = tr.endPos;
            continue;
        }

        // Bounce: reflect off surface, lose energy
        g.origin = tr.endPos;
        float vDotN = g.velocity.Dot(tr.normal);
        Vec3 reflected = g.velocity - tr.normal * (2.0f * vDotN);
        g.velocity = reflected * GrenadeParams::BOUNCE_DAMPING;
        g.bounceCount++;

        // Settle on near-horizontal surfaces at low speed
        if (tr.normal.z >= 0.7f &&
            g.velocity.Length() < GrenadeParams::REST_SPEED) {
            g.velocity = {};
            g.atRest   = true;
        }
    }

    // Compact out detonated grenades
    m_grenades.erase(
        std::remove_if(m_grenades.begin(), m_grenades.end(),
                       [](const Grenade& g) { return g.detonated; }),
        m_grenades.end());
}

float GrenadeSystem::HEDamage(float distance) {
    if (distance >= GrenadeParams::HE_RADIUS) return 0;
    float frac = 1.0f - distance / GrenadeParams::HE_RADIUS;
    return GrenadeParams::HE_MAX_DAMAGE * frac;
}

} // namespace OS
