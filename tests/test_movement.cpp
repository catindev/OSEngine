// Unit tests for CS 1.6 player movement physics.
// Verifies behavioral constants match expected CS 1.6 behavior.

#include "../src/physics/Movement.h"
#include "../src/physics/Collision.h"
#include <cassert>
#include <cstdio>
#include <cmath>

using namespace OS;

// Trace function for open space (no geometry)
static TraceResult OpenTrace(Vec3 start, Vec3 end, Vec3, Vec3, int) {
    TraceResult tr;
    tr.fraction = 1.0f;
    tr.endPos   = end;
    tr.hit      = false;
    return tr;
}

// Trace that has a floor at z=0
static TraceResult FloorTrace(Vec3 start, Vec3 end, Vec3 hullMins, Vec3 hullMaxs, int) {
    TraceResult tr;
    tr.fraction = 1.0f;
    tr.endPos   = end;
    tr.hit      = false;

    float floorZ   = 0.0f;
    float startBot = start.z + hullMins.z;
    float endBot   = end.z   + hullMins.z;
    float dz       = startBot - endBot;

    // Only intersect if moving downward through the floor
    if (dz > 1e-6f && startBot >= floorZ && endBot < floorZ) {
        float frac = (startBot - floorZ) / dz;
        frac = frac < 0 ? 0 : (frac > 1 ? 1 : frac);
        tr.fraction = frac;
        tr.hit      = true;
        tr.normal   = {0, 0, 1};
        tr.endPos   = start + (end - start) * frac;
    }
    return tr;
}

static void Assert(bool cond, const char* msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        exit(1);
    }
    printf("PASS: %s\n", msg);
}

static void TestMaxSpeed() {
    MoveVars mv;
    PlayerMove pm(mv, FloorTrace);
    PlayerState s;
    s.origin    = {0, 0, 36}; // standing on floor at z=0, hull bottom = z-36 = 0
    s.onGround  = true;
    s.velocity  = {};

    PlayerInput inp;
    inp.wishDir   = {1, 0, 0};
    inp.wishSpeed = mv.maxspeed;

    // Run for 1 second (many small steps)
    float dt = 1.0f / 64.0f;
    for (int i = 0; i < 64; i++)
        pm.Move(s, inp, dt);

    float speed = s.velocity.Length2D();
    Assert(speed <= mv.maxspeed + 0.01f,
           "Ground speed does not exceed sv_maxspeed");
    Assert(speed > mv.maxspeed * 0.95f,
           "Ground speed reaches near sv_maxspeed after 1s of acceleration");

    printf("  Ground speed after 1s: %.2f / %.2f\n", speed, mv.maxspeed);
}

static void TestJumpHeight() {
    MoveVars mv;
    PlayerMove pm(mv, FloorTrace);
    PlayerState s;
    s.origin   = {0, 0, 36};
    s.onGround = true;
    s.velocity = {};

    PlayerInput inp;
    inp.wishDir   = {};
    inp.wishSpeed = 0;
    inp.jumpPressed = true;

    float dt = 1.0f / 64.0f;
    pm.Move(s, inp, dt); // jump

    float peakZ = s.origin.z;
    inp.jumpPressed = false;

    // Simulate until we land
    int iters = 0;
    while (!s.onGround && iters++ < 1000) {
        pm.Move(s, inp, dt);
        if (s.origin.z > peakZ) peakZ = s.origin.z;
    }

    float jumpHeight = peakZ - 36.0f; // subtract starting Z
    printf("  Jump height: %.2f units (expected ~45)\n", jumpHeight);
    Assert(jumpHeight >= 40.0f && jumpHeight <= 55.0f,
           "Jump height approximately 45 units (CS 1.6 reference)");
}

static void TestFrictionDeceleration() {
    MoveVars mv;
    PlayerMove pm(mv, FloorTrace);
    PlayerState s;
    s.origin   = {0, 0, 36};
    s.onGround = true;
    s.velocity = {mv.maxspeed, 0, 0};

    PlayerInput inp;
    inp.wishDir   = {};
    inp.wishSpeed = 0;

    float dt = 1.0f / 64.0f;
    // Apply friction for 0.5 seconds
    for (int i = 0; i < 32; i++)
        pm.Move(s, inp, dt);

    float speed = s.velocity.Length();
    printf("  Speed after 0.5s friction: %.2f\n", speed);
    Assert(speed < mv.maxspeed * 0.5f,
           "Friction decelerates player significantly within 0.5s");
}

static void TestAirStrafe() {
    MoveVars mv;
    PlayerMove pm(mv, OpenTrace);
    PlayerState s;
    s.origin   = {0, 0, 36};
    s.onGround = false;
    s.velocity = {mv.maxspeed, 0, 0}; // already at max speed, moving forward

    // Strafe right in air — should build speed above maxspeed (bhop mechanics)
    PlayerInput inp;
    inp.wishDir   = {0, 1, 0}; // strafe right
    inp.wishSpeed = mv.maxspeed;

    float dt = 1.0f / 64.0f;
    float prevSpeed = s.velocity.Length2D();

    for (int i = 0; i < 16; i++) {
        pm.Move(s, inp, dt);
    }

    float newSpeed = s.velocity.Length2D();
    printf("  Air strafe speed delta: %.2f -> %.2f\n", prevSpeed, newSpeed);
    // Air strafing with perfect angle should slightly increase speed
    Assert(newSpeed >= prevSpeed - 1.0f,
           "Air strafing does not significantly reduce speed");
}

static void TestFallDamage() {
    float dmg0   = PlayerMove::CalcFallDamage(200.0f);
    float dmg580 = PlayerMove::CalcFallDamage(580.0f);
    float dmg800 = PlayerMove::CalcFallDamage(800.0f);

    Assert(dmg0   == 0.0f, "No fall damage at 200 u/s");
    Assert(dmg580 == 0.0f, "No fall damage at threshold (580 u/s)");
    Assert(dmg800 >  0.0f, "Fall damage at 800 u/s");

    printf("  Fall damage at 800 u/s: %.2f HP\n", dmg800);
}

int main() {
    printf("=== Movement Tests ===\n\n");

    TestMaxSpeed();
    TestJumpHeight();
    TestFrictionDeceleration();
    TestAirStrafe();
    TestFallDamage();

    printf("\nAll movement tests passed.\n");
    return 0;
}
