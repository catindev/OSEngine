#include "Movement.h"
#include <cmath>
#include <algorithm>

namespace OS {

// ─── CS 1.6 jump velocity (approximate from community measurements) ──────────
static constexpr float JUMP_IMPULSE = 268.3281572f; // sqrt(2 * 800 * 45) ≈ upward vel for ~45u jump height

// ─── Duck timing ─────────────────────────────────────────────────────────────
static constexpr float DUCK_TIME     = 0.4f;   // seconds to fully duck
static constexpr float UNDUCK_TIME   = 0.2f;   // seconds to un-duck

// ─── Edge friction kick ───────────────────────────────────────────────────────
// When near a ledge edge, extra friction applied (GoldSrc behavior)
static constexpr float EDGE_FRICTION_DIST = 16.0f;

// ─── Helper ───────────────────────────────────────────────────────────────────


Vec3 PlayerMove::HullMins(const PlayerState& s) const {
    return s.ducking ? m_mv.duckHullMins : m_mv.hullMins;
}

Vec3 PlayerMove::HullMaxs(const PlayerState& s) const {
    return s.ducking ? m_mv.duckHullMaxs : m_mv.hullMaxs;
}

Vec3 PlayerMove::ClipVelocity(Vec3 vel, Vec3 normal, float overbounce) const {
    float backoff = vel.Dot(normal) * overbounce;
    Vec3 out = vel - normal * backoff;
    // Remove tiny floating-point residuals
    for (int i = 0; i < 3; i++)
        if (std::abs(out[i]) < 1e-4f) out[i] = 0;
    return out;
}

void PlayerMove::ClampVelocity(PlayerState& s) {
    float speed = s.velocity.Length();
    if (speed > m_mv.maxvelocity) {
        s.velocity = s.velocity * (m_mv.maxvelocity / speed);
    }
}

// ─── Friction ─────────────────────────────────────────────────────────────────
// Quake-style friction, reproduced as used in GoldSrc:
//   1. Compute speed.
//   2. If speed is below stopspeed, use stopspeed as control speed.
//   3. drop = control * friction * dt
//   4. newspeed = max(0, speed - drop) / speed

void PlayerMove::ApplyFriction(PlayerState& s, float dt) {
    float speed = s.velocity.Length();
    if (speed < 0.1f) {
        s.velocity = {};
        return;
    }

    float drop = 0;

    if (s.onGround) {
        float control = speed < m_mv.stopspeed ? m_mv.stopspeed : speed;
        drop = control * m_mv.friction * dt;
    }

    float newspeed = speed - drop;
    if (newspeed < 0) newspeed = 0;
    if (newspeed != speed)
        s.velocity *= newspeed / speed;
}

// ─── Accelerate ───────────────────────────────────────────────────────────────
// Quake-style ground acceleration.

void PlayerMove::Accelerate(PlayerState& s, Vec3 wishDir, float wishSpeed,
                             float accel, float dt) {
    float currentspeed = s.velocity.Dot(wishDir);
    float addspeed = wishSpeed - currentspeed;
    if (addspeed <= 0) return;

    float accelspeed = accel * dt * wishSpeed;
    if (accelspeed > addspeed) accelspeed = addspeed;

    s.velocity += wishDir * accelspeed;
}

// ─── Air accelerate ───────────────────────────────────────────────────────────
// CS 1.6 air acceleration: wishSpeed capped to 30 units before computing addspeed.
// This is the mechanic that enables air-strafing and bunny hopping.

void PlayerMove::AirAccelerate(PlayerState& s, Vec3 wishDir, float wishSpeed,
                                float accel, float dt) {
    float wishspd = std::min(wishSpeed, 30.0f);  // CS 1.6 air cap

    float currentspeed = s.velocity.Dot(wishDir);
    float addspeed = wishspd - currentspeed;
    if (addspeed <= 0) return;

    float accelspeed = accel * wishSpeed * dt;
    if (accelspeed > addspeed) accelspeed = addspeed;

    s.velocity += wishDir * accelspeed;
}

void PlayerMove::WaterAccelerate(PlayerState& s, Vec3 wishDir, float wishSpeed, float dt) {
    Accelerate(s, wishDir, wishSpeed, m_mv.wateraccelerate, dt);
}

// ─── Ground check ─────────────────────────────────────────────────────────────

void PlayerMove::CheckGround(PlayerState& s) {
    // Trace slightly below the player
    Vec3 start = s.origin;
    Vec3 end   = s.origin + Vec3(0, 0, -2.0f);

    TraceResult tr = Trace(start, end, s);

    if (tr.fraction < 1.0f && tr.normal.z >= 0.7f) {
        s.onGround     = true;
        s.groundNormal = tr.normal;
        // Snap to ground surface
        s.origin = tr.endPos;
    } else {
        s.onGround     = false;
        s.groundNormal = {0,0,1};
    }
}

// ─── CategorizePosition ───────────────────────────────────────────────────────

void PlayerMove::CategorizePosition(PlayerState& s) {
    if (s.velocity.z > 180.0f) {
        // Moving fast upward: not on ground (prevents stick on jump)
        s.onGround = false;
        return;
    }
    CheckGround(s);
}

// ─── Water check ──────────────────────────────────────────────────────────────

bool PlayerMove::CheckWater(PlayerState& s) {
    // Sample at three heights for water level
    Vec3 point = s.origin;
    point.z += HullMins(s).z + 1.0f; // feet

    // TODO: query BSP contents for water detection
    // For now, water detection is a stub that always returns false
    s.inWater    = false;
    s.waterLevel = 0;
    return false;
}

// ─── Duck / Crouch ────────────────────────────────────────────────────────────

void PlayerMove::Duck(PlayerState& s, const PlayerInput& in, float dt) {
    if (in.duckPressed) {
        if (!s.ducking) {
            s.duckProgress += dt / DUCK_TIME;
            if (s.duckProgress >= 1.0f) {
                s.duckProgress = 1.0f;
                s.ducking = true;
                // Drop player to crouch origin (GoldSrc shifts origin by hull height change)
                float heightDelta = (m_mv.hullMaxs.z - m_mv.duckHullMaxs.z) * 0.5f;
                s.origin.z -= heightDelta;
            }
        }
    } else {
        if (s.ducking || s.duckProgress > 0) {
            // Check if there's room to stand
            Vec3 testEnd = s.origin + Vec3(0, 0, m_mv.hullMaxs.z - m_mv.duckHullMaxs.z);
            TraceResult tr = m_trace(s.origin, testEnd,
                m_mv.hullMins, m_mv.hullMaxs, MASK_SOLID);
            if (!tr.hit && !tr.startSolid) {
                s.duckProgress -= dt / UNDUCK_TIME;
                if (s.duckProgress <= 0.0f) {
                    s.duckProgress = 0.0f;
                    if (s.ducking) {
                        float heightDelta = (m_mv.hullMaxs.z - m_mv.duckHullMaxs.z) * 0.5f;
                        s.origin.z += heightDelta;
                        s.ducking = false;
                    }
                }
            }
        }
    }
}

// ─── Jump ─────────────────────────────────────────────────────────────────────

void PlayerMove::Jump(PlayerState& s, const PlayerInput& in) {
    if (!in.jumpPressed) return;
    if (!s.onGround)     return;
    if (s.inWater)       return;

    // GoldSrc applies a fixed upward velocity and removes ground contact
    s.velocity.z = JUMP_IMPULSE;
    s.onGround = false;

    // Ducking while jumping gives a slight extra boost (CS 1.6 duck-jump)
    // Not adding this yet — verify from recordings first
}

// ─── Slide (recursive Quake-style) ───────────────────────────────────────────

bool PlayerMove::SlideMove(PlayerState& s, float dt, bool gravity) {
    static constexpr int MAX_BUMPS  = 4;
    static constexpr float OVERBOUNCE = 1.0f;

    Vec3 planes[MAX_BUMPS];
    int  numPlanes = 0;

    Vec3 vel = s.velocity;
    if (gravity) vel.z -= m_mv.gravity * dt * 0.5f;

    float time = dt;
    for (int bump = 0; bump < MAX_BUMPS && time > 0; bump++) {
        Vec3 end = s.origin + vel * time;
        TraceResult tr = Trace(s.origin, end, s);

        if (tr.allSolid) break;
        if (tr.fraction > 0) {
            s.origin = tr.endPos;
            numPlanes = 0;
        }
        if (tr.fraction == 1.0f) break;

        time -= time * tr.fraction;
        if (numPlanes >= MAX_BUMPS) break;

        planes[numPlanes++] = tr.normal;

        // Clip velocity to all collected planes
        Vec3 clipped = vel;
        for (int i = 0; i < numPlanes; i++)
            clipped = ClipVelocity(clipped, planes[i], OVERBOUNCE);

        // Check for colliding with multiple planes simultaneously
        bool allBack = true;
        for (int i = 0; i < numPlanes; i++) {
            if (clipped.Dot(planes[i]) >= 0) { allBack = false; break; }
        }
        if (allBack) {
            // Moving into all planes: crossproduct corner slide
            if (numPlanes >= 2) {
                Vec3 dir = planes[0].Cross(planes[1]).Normalized();
                float d = dir.Dot(vel);
                clipped = dir * d;
            } else {
                clipped = {};
            }
        }

        vel = clipped;
    }

    if (gravity) {
        s.velocity.z -= m_mv.gravity * dt * 0.5f;
        vel.z = s.velocity.z;
    }

    s.velocity = vel;
    return numPlanes == 0;
}

// ─── Step-up helper ───────────────────────────────────────────────────────────
// Tries to step up over ledges of height <= stepsize.

TraceResult PlayerMove::StepMove(PlayerState& s, Vec3 wishVelocity, float dt) {
    Vec3 originalOrigin = s.origin;
    Vec3 originalVelocity = s.velocity;

    // Try direct move first
    Vec3 end1 = s.origin + wishVelocity * dt;
    TraceResult tr = Trace(s.origin, end1, s);
    if (tr.fraction == 1.0f) {
        s.origin = tr.endPos;
        return tr;
    }

    // Try with step-up
    Vec3 upOrigin = s.origin + Vec3(0, 0, m_mv.stepsize);
    TraceResult upTr = Trace(s.origin, upOrigin, s);
    if (!upTr.allSolid) {
        s.origin = upTr.endPos;
        Vec3 end2 = s.origin + wishVelocity * dt;
        TraceResult fwdTr = Trace(s.origin, end2, s);

        // Step down
        Vec3 downEnd = fwdTr.endPos + Vec3(0, 0, -m_mv.stepsize * 2);
        TraceResult downTr = Trace(fwdTr.endPos, downEnd, s);

        if (!downTr.allSolid && downTr.fraction < 1.0f && downTr.normal.z >= 0.7f) {
            // Stepped up successfully
            s.origin = downTr.endPos;
            // Preserve horizontal velocity component
            s.velocity.x = wishVelocity.x;
            s.velocity.y = wishVelocity.y;
            return downTr;
        }
    }

    // Revert to original
    s.origin   = originalOrigin;
    s.velocity = originalVelocity;
    return tr;
}

// ─── Ground movement ──────────────────────────────────────────────────────────

void PlayerMove::GroundMove(PlayerState& s, const PlayerInput& in, float dt) {
    s.velocity.z = 0; // zero out any downward velocity while on ground

    Vec3 wishVel  = in.wishDir * in.wishSpeed;
    Vec3 wishDir  = wishVel.Normalized();
    float wishSpd = wishVel.Length();

    // Clamp to maxspeed
    if (wishSpd > m_mv.maxspeed) {
        wishVel  *= m_mv.maxspeed / wishSpd;
        wishSpd   = m_mv.maxspeed;
    }

    ApplyFriction(s, dt);
    Accelerate(s, wishDir, wishSpd, m_mv.accelerate, dt);

    // Align velocity to ground plane
    if (s.groundNormal.z < 1.0f - kEpsilon) {
        Vec3 down = ClipVelocity(s.velocity, s.groundNormal);
        if (down.z < s.velocity.z) s.velocity = down;
    }

    // Attempt to step up and move
    Vec3 moveVel = s.velocity;
    Vec3 end = s.origin + moveVel * dt;
    TraceResult tr = Trace(s.origin, end, s);

    if (tr.fraction < 1.0f) {
        // Hit something — try to step up
        StepMove(s, moveVel, dt);
    } else {
        s.origin = tr.endPos;
    }

    // Stick to ground (pull down if falling off a ledge)
    Vec3 downEnd = s.origin + Vec3(0, 0, -m_mv.stepsize);
    TraceResult down = Trace(s.origin, downEnd, s);
    if (down.fraction < 1.0f && down.normal.z >= 0.7f && !down.startSolid) {
        s.origin     = down.endPos;
        s.onGround   = true;
        s.groundNormal = down.normal;
    }
}

// ─── Air movement ─────────────────────────────────────────────────────────────

void PlayerMove::AirMove(PlayerState& s, const PlayerInput& in, float dt) {
    Vec3 wishVel  = in.wishDir * in.wishSpeed;
    Vec3 wishDir  = wishVel.Normalized();
    float wishSpd = wishVel.Length();

    if (wishSpd > m_mv.maxspeed) {
        wishVel *= m_mv.maxspeed / wishSpd;
        wishSpd  = m_mv.maxspeed;
    }

    AirAccelerate(s, wishDir, wishSpd, m_mv.airaccelerate, dt);

    // Apply gravity (half before move, half after — Quake integration)
    s.velocity.z -= m_mv.gravity * dt;

    SlideMove(s, dt, false);
}

// ─── Water movement ───────────────────────────────────────────────────────────

void PlayerMove::WaterMove(PlayerState& s, const PlayerInput& in, float dt) {
    Vec3 wishDir  = in.wishDir;
    float wishSpd = in.wishSpeed;
    if (wishSpd > m_mv.maxspeed) {
        wishDir  = wishDir.Normalized() * m_mv.maxspeed;
        wishSpd  = m_mv.maxspeed;
    }

    WaterAccelerate(s, wishDir, wishSpd, dt);

    // Dampen
    s.velocity *= std::max(0.0f, 1.0f - m_mv.waterfriction * dt);
    ClampVelocity(s);

    Vec3 end = s.origin + s.velocity * dt;
    TraceResult tr = Trace(s.origin, end, s);
    s.origin = tr.endPos;
    if (tr.hit) s.velocity = ClipVelocity(s.velocity, tr.normal);
}

// ─── Ladder movement ──────────────────────────────────────────────────────────

void PlayerMove::LadderMove(PlayerState& s, const PlayerInput& in, float dt) {
    Vec3 wishDir;
    wishDir.x = in.wishDir.x;
    wishDir.y = in.wishDir.y;
    wishDir.z = in.upMove;
    wishDir = wishDir.Normalized();

    float wishSpd = m_mv.maxspeed;
    Accelerate(s, wishDir, wishSpd, m_mv.accelerate, dt);

    s.velocity.z = wishDir.z * wishSpd;
    s.velocity *= std::max(0.0f, 1.0f - m_mv.friction * dt);

    Vec3 end = s.origin + s.velocity * dt;
    TraceResult tr = Trace(s.origin, end, s);
    s.origin = tr.endPos;
}

// ─── Main Move ────────────────────────────────────────────────────────────────

void PlayerMove::Move(PlayerState& s, const PlayerInput& in, float dt) {
    ClampVelocity(s);
    Duck(s, in, dt);

    if (s.onLadder || in.onLadder) {
        LadderMove(s, in, dt);
        CategorizePosition(s);
        return;
    }

    CheckWater(s);

    if (s.inWater && s.waterLevel >= 2) {
        WaterMove(s, in, dt);
        CategorizePosition(s);
        return;
    }

    // Jump (only when on ground)
    Jump(s, in);

    if (s.onGround) {
        GroundMove(s, in, dt);
    } else {
        AirMove(s, in, dt);
    }

    CategorizePosition(s);
    ClampVelocity(s);
}

// ─── Fall damage ──────────────────────────────────────────────────────────────
// CS 1.6 fall damage: >580 units/s = damage, each unit beyond 580 = ~1 HP

float PlayerMove::CalcFallDamage(float impactVelocity) {
    static constexpr float FALL_DAMAGE_THRESHOLD = 580.0f;
    static constexpr float DAMAGE_PER_UNIT = 0.17f; // approximate
    float excess = std::abs(impactVelocity) - FALL_DAMAGE_THRESHOLD;
    if (excess <= 0) return 0;
    return excess * DAMAGE_PER_UNIT;
}

} // namespace OS
