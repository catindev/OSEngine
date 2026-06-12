#pragma once
// CS 1.6 / GoldSrc player movement physics
//
// Reimplemented from scratch by observing CS 1.6 behavior.
// Constants and behavior derived from publicly documented GoldSrc pmove specs
// and community movement research.  No Valve code used.
//
// Key references:
//   - GoldSrc movement documentation by community researchers
//   - Bunnyhopping physics analysis (public forum posts, no proprietary content)
//   - Valve Developer Community wiki (public documentation)

#include <functional>
#include "../math/Math.h"

namespace OS {

// ─── Movement constants (CS 1.6 defaults) ─────────────────────────────────────

struct MoveVars {
    float gravity        = 800.0f;
    float stopspeed      = 100.0f;
    float maxspeed       = 320.0f;
    float spectatormaxspeed = 500.0f;
    float accelerate     = 10.0f;
    float airaccelerate  = 10.0f;
    float wateraccelerate = 10.0f;
    float friction       = 4.0f;
    float edgefriction   = 2.0f;
    float waterfriction  = 1.0f;
    float entgravity     = 1.0f;
    float bounce         = 1.0f;
    float stepsize       = 18.0f;
    float maxvelocity    = 2000.0f;
    float zmax           = 1024.0f;

    // Player hull dimensions (standing)
    Vec3 hullMins = { -16, -16, -36 };
    Vec3 hullMaxs = {  16,  16,  36 };

    // Player hull dimensions (crouching)
    Vec3 duckHullMins = { -16, -16, -18 };
    Vec3 duckHullMaxs = {  16,  16,  18 };

    // Eye height offsets from origin
    float eyeHeightStand = 28.0f;
    float eyeHeightDuck  = 12.0f;
};

// Player input state for one frame
struct PlayerInput {
    Vec3  wishDir = {};     // normalized movement direction (from keys)
    float wishSpeed = 0;    // desired speed (before clamping)
    bool  jumpPressed  = false; // current jump key state (held)
    bool  duckPressed  = false;
    bool  walkPressed  = false; // shift walk (CS 1.6: 0.52x speed)
    bool  onLadder     = false;
    float forwardMove  = 0; // -1..1
    float sideMove     = 0; // -1..1
    float upMove       = 0; // -1..1 (for water/ladder)
};

// Player physics state
struct PlayerState {
    Vec3  origin;
    Vec3  velocity;
    Angles angles;

    bool  onGround     = false;
    bool  ducking      = false;
    bool  inWater      = false;
    bool  onLadder     = false;

    float duckProgress = 0.0f; // 0..1  (animation progress)
    float waterLevel   = 0.0f; // 0=none, 1=feet, 2=waist, 3=head
    float fallVelocity = 0.0f; // velocity at last ground contact (for fall damage)

    Vec3  groundNormal = {0,0,1};
    int   groundContents = 0;

    // Jump latch: GoldSrc requires releasing jump between hops
    bool  jumpHeldLast = false;

    // GoldSrc-style stamina / speed modifiers
    float stamina = 0.0f;  // reduces max speed when running (0=no penalty)
};

// CS 1.6 movement speed factors
static constexpr float DUCK_SPEED_FACTOR = 0.333f;
static constexpr float WALK_SPEED_FACTOR = 0.52f;

// Trace result for collision
struct TraceResult {
    bool  startSolid  = false; // started inside geometry
    bool  allSolid    = false; // entirely inside geometry
    bool  hit         = false;
    float fraction    = 1.0f;  // 0..1, how far the move went
    Vec3  endPos;
    Vec3  normal;
    int   contents    = 0;     // BSPContents of hit surface
    int   hitEntity   = -1;    // entity index, -1 = world
};

// Collision callback — provided by engine, calls into BSP/entity system
using TraceFn = std::function<TraceResult(
    Vec3 start, Vec3 end,
    Vec3 hullMins, Vec3 hullMaxs,
    int contentsMask
)>;

// Contents mask for player movement
static constexpr int MASK_SOLID     = 0x01; // CONTENTS_SOLID
static constexpr int MASK_LADDER    = 0x02; // CONTENTS_LADDER
static constexpr int MASK_WATER     = 0x04; // CONTENTS_WATER|SLIME|LAVA
static constexpr int MASK_PLAYERSOLID = MASK_SOLID;

// ─── Movement processor ───────────────────────────────────────────────────────

class PlayerMove {
public:
    PlayerMove(const MoveVars& mv, TraceFn trace)
        : m_mv(mv), m_trace(std::move(trace)) {}

    // Process one frame of movement.
    // dt: frame time in seconds (typically 1/64 on server, variable on client)
    void Move(PlayerState& state, const PlayerInput& input, float dt);

    // Check if the player is on the ground (and update state.onGround)
    void CheckGround(PlayerState& state);

    // Compute the velocity fall damage would deal given impact velocity
    static float CalcFallDamage(float impactVelocity);

private:
    MoveVars m_mv;
    TraceFn  m_trace;

    // ─── Sub-systems ──────────────────────────────────────────────────────────

    void ApplyFriction  (PlayerState& s, float dt);
    void Accelerate     (PlayerState& s, Vec3 wishDir, float wishSpeed, float accel, float dt);
    void AirAccelerate  (PlayerState& s, Vec3 wishDir, float wishSpeed, float accel, float dt);
    void WaterAccelerate(PlayerState& s, Vec3 wishDir, float wishSpeed, float dt);

    void GroundMove(PlayerState& s, const PlayerInput& in, float dt);
    void AirMove   (PlayerState& s, const PlayerInput& in, float dt);
    void WaterMove (PlayerState& s, const PlayerInput& in, float dt);
    void LadderMove(PlayerState& s, const PlayerInput& in, float dt);

    void Duck    (PlayerState& s, const PlayerInput& in, float dt);
    void Jump    (PlayerState& s, const PlayerInput& in);

    void ClampVelocity(PlayerState& s);
    bool CheckWater   (PlayerState& s);
    void CategorizePosition(PlayerState& s);

    // Step up over small ledges
    TraceResult StepMove(PlayerState& s, Vec3 wishVelocity, float dt);

    // Slide movement (Quake-style recursive)
    Vec3 ClipVelocity(Vec3 vel, Vec3 normal, float overbounce = 1.0f) const;
    bool SlideMove(PlayerState& s, float dt, bool gravity);

    Vec3 HullMins(const PlayerState& s) const;
    Vec3 HullMaxs(const PlayerState& s) const;

    TraceResult Trace(Vec3 start, Vec3 end, const PlayerState& s) const {
        return m_trace(start, end, HullMins(s), HullMaxs(s), MASK_PLAYERSOLID);
    }
};

} // namespace OS
