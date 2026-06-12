#include "WeaponSystem.h"
#include <cmath>
#include <algorithm>

namespace OS {

float HitGroupMultiplier(HitGroup g) {
    switch (g) {
    case HITGROUP_HEAD:     return 4.0f;
    case HITGROUP_CHEST:    return 1.0f;
    case HITGROUP_STOMACH:  return 1.25f;
    case HITGROUP_LEFTARM:
    case HITGROUP_RIGHTARM: return 1.0f;
    case HITGROUP_LEFTLEG:
    case HITGROUP_RIGHTLEG: return 0.75f;
    default:                return 1.0f;
    }
}

WeaponSystem::WeaponSystem() : m_rng(0x5EED) {}

void WeaponSystem::SeedRandom(uint32_t seed) { m_rng.seed(seed); }

float WeaponSystem::Rand11() {
    return std::uniform_real_distribution<float>(-1.0f, 1.0f)(m_rng);
}

WeaponState WeaponSystem::MakeWeapon(WeaponID id) {
    WeaponState ws;
    ws.id = id;
    const WeaponDef* def = WeaponDatabase::Get(id);
    if (def) {
        ws.clip    = def->clipSize;
        ws.reserve = def->maxAmmo;
    }
    return ws;
}

float WeaponSystem::FalloffDamage(float baseDamage, float rangeModifier, float distance) {
    if (rangeModifier <= 0 || rangeModifier >= 1.0f)
        return baseDamage;
    return baseDamage * std::pow(rangeModifier, distance / 500.0f);
}

float WeaponSystem::ComputeSpread(const WeaponDef& def, const ShooterContext& ctx,
                                   const WeaponState& ws) const {
    const SpreadParams& sp = def.spread;

    float spread;
    if (!ctx.onGround)
        spread = sp.jumping;
    else if (ctx.velocity2D > 80.0f)
        spread = sp.moving;
    else if (ctx.ducking)
        spread = sp.crouching;
    else
        spread = sp.standStill;

    // Sustained fire adds spread (recoil accumulation proxy)
    spread += ws.recoilPitch * 0.15f;

    // Spread values stored as "inaccuracy units" — scale to degrees.
    // CS-style: 1 unit ≈ 0.022° half-angle at typical ranges (tuned empirically).
    return spread * 0.05f;
}

BulletHit WeaponSystem::TraceBullet(Vec3 start, Vec3 dir, const WeaponDef& def,
                                     const TraceFn& trace,
                                     const std::vector<ShootTarget>& targets) {
    BulletHit hit;

    float maxRange = def.range > 0 ? def.range : 8192.0f;
    Vec3 end = start + dir * maxRange;

    // World trace (point hull)
    TraceResult tr = trace(start, end, Vec3{}, Vec3{}, MASK_SOLID);
    float worldDist = tr.hit ? (tr.endPos - start).Length() : maxRange;

    // Target sweep: closest AABB intersection wins (if closer than world)
    float bestDist = worldDist;
    int   bestId   = -1;
    HitGroup bestGroup = HITGROUP_GENERIC;
    Vec3  bestPoint;

    Ray ray(start, dir);
    for (const ShootTarget& t : targets) {
        if (!t.alive) continue;

        float tMin, tMax;
        // Head first (higher priority on overlap)
        if (ray.IntersectsAABB(t.head, tMin, tMax) && tMin >= 0 && tMin < bestDist) {
            bestDist  = tMin;
            bestId    = t.id;
            bestGroup = HITGROUP_HEAD;
            bestPoint = ray.At(tMin);
            continue;
        }
        if (ray.IntersectsAABB(t.body, tMin, tMax) && tMin >= 0 && tMin < bestDist) {
            bestDist  = tMin;
            bestId    = t.id;
            // Split body box by height: top third chest, middle stomach, bottom legs
            Vec3 p = ray.At(tMin);
            float frac = (p.z - t.body.mins.z) / std::max(1.0f, t.body.Size().z);
            bestGroup = frac > 0.66f ? HITGROUP_CHEST :
                        frac > 0.33f ? HITGROUP_STOMACH : HITGROUP_LEFTLEG;
            bestPoint = p;
        }
    }

    if (bestId >= 0) {
        hit.hitTarget = true;
        hit.targetId  = bestId;
        hit.hitGroup  = bestGroup;
        hit.point     = bestPoint;
        hit.distance  = bestDist;
        hit.damage    = FalloffDamage(def.damage, def.rangeModifier, bestDist)
                      * HitGroupMultiplier(bestGroup);
    } else if (tr.hit) {
        hit.hitWorld = true;
        hit.point    = tr.endPos;
        hit.normal   = tr.normal;
        hit.distance = worldDist;
    }

    return hit;
}

FireEvent WeaponSystem::TryFire(WeaponState& ws, const ShooterContext& ctx,
                                 bool triggerHeld, bool triggerPressed,
                                 const TraceFn& trace,
                                 const std::vector<ShootTarget>& targets) {
    FireEvent ev;

    const WeaponDef* def = WeaponDatabase::Get(ws.id);
    if (!def || def->fireRate <= 0) return ev;

    // Semi-auto requires fresh trigger press
    bool wantFire = (def->fireMode == FIRE_AUTO) ? triggerHeld : triggerPressed;
    if (!wantFire) return ev;

    if (ws.reloading)          return ev;
    if (ctx.time < ws.nextFire) return ev;

    if (ws.clip <= 0) {
        ev.dryFire = true;
        ws.nextFire = ctx.time + 0.2f; // dry-fire click rate
        return ev;
    }

    // Fire!
    ws.clip--;
    ws.nextFire = ctx.time + 1.0f / def->fireRate;

    float spreadDeg = ComputeSpread(*def, ctx, ws);

    Vec3 forward, right, up;
    ctx.viewAngles.AngleVectors(&forward, &right, &up);

    int pellets = std::max(1, def->bulletsPerShot);
    for (int b = 0; b < pellets; b++) {
        // Triangle-ish distribution (sum of two uniforms), like GoldSrc shared rand
        float rx = (Rand11() + Rand11()) * 0.5f;
        float ry = (Rand11() + Rand11()) * 0.5f;

        float spreadRad = DegToRad(spreadDeg);
        Vec3 dir = (forward
                  + right * (rx * spreadRad)
                  + up    * (ry * spreadRad)).Normalized();

        ev.hits.push_back(TraceBullet(ctx.eyePos, dir, *def, trace, targets));
    }

    // Recoil kick
    const RecoilParams& rc = def->recoil;
    float kickPitch = rc.minRecoilPitch +
        (rc.maxRecoilPitch - rc.minRecoilPitch) * (Rand11() * 0.5f + 0.5f);
    float kickYaw = rc.minRecoilYaw +
        (rc.maxRecoilYaw - rc.minRecoilYaw) * (Rand11() * 0.5f + 0.5f);

    ws.recoilPitch += kickPitch;
    ws.recoilYaw   += kickYaw;

    ev.fired       = true;
    ev.bulletsFired = pellets;
    ev.punchPitch  = kickPitch;
    ev.punchYaw    = kickYaw;
    return ev;
}

bool WeaponSystem::StartReload(WeaponState& ws, float time) {
    const WeaponDef* def = WeaponDatabase::Get(ws.id);
    if (!def || def->clipSize <= 0) return false;
    if (ws.reloading)               return false;
    if (ws.clip >= def->clipSize)   return false;
    if (ws.reserve <= 0)            return false;

    ws.reloading  = true;
    ws.nextReload = time + def->reloadTime;
    ws.zoomed     = false;
    ws.zoomLevel  = 0;
    return true;
}

void WeaponSystem::Update(WeaponState& ws, float time, float dt) {
    const WeaponDef* def = WeaponDatabase::Get(ws.id);
    if (!def) return;

    // Complete reload
    if (ws.reloading && time >= ws.nextReload) {
        int need = def->clipSize - ws.clip;
        int take = std::min(need, ws.reserve);
        ws.clip    += take;
        ws.reserve -= take;
        ws.reloading = false;
    }

    // Recoil recovery
    float recovery = def->recoil.recoveryRate;
    if (recovery <= 0) recovery = 10.0f;
    float decay = recovery * dt;

    if (ws.recoilPitch > 0) ws.recoilPitch = std::max(0.0f, ws.recoilPitch - decay);
    if (ws.recoilYaw   > 0) ws.recoilYaw   = std::max(0.0f, ws.recoilYaw   - decay);
    if (ws.recoilYaw   < 0) ws.recoilYaw   = std::min(0.0f, ws.recoilYaw   + decay);
}

} // namespace OS
