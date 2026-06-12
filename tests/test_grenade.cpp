// Grenade physics tests: throw velocity, gravity arc, bounce, fuse timing,
// HE damage falloff.

#include "../src/game/Projectile.h"
#include <cassert>
#include <cstdio>
#include <cmath>

using namespace OS;

static void Assert(bool cond, const char* msg) {
    if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); exit(1); }
    printf("PASS: %s\n", msg);
}

static TraceResult OpenTrace(Vec3 start, Vec3 end, Vec3, Vec3, int) {
    TraceResult tr;
    tr.fraction = 1.0f;
    tr.endPos   = end;
    return tr;
}

// Floor at z = 0
static TraceResult FloorTrace(Vec3 start, Vec3 end, Vec3, Vec3, int) {
    TraceResult tr;
    tr.fraction = 1.0f;
    tr.endPos   = end;

    if (start.z >= 0 && end.z < 0) {
        float frac = start.z / (start.z - end.z);
        tr.fraction = frac;
        tr.hit      = true;
        tr.normal   = {0, 0, 1};
        tr.endPos   = start + (end - start) * frac;
    }
    return tr;
}

static void TestThrowVelocity() {
    // Looking straight ahead (pitch 0): speed = 90*6 = 540
    Vec3 v0 = GrenadeSystem::ThrowVelocity({0, 0, 0}, {});
    printf("  Throw speed at pitch 0: %.1f\n", v0.Length());
    Assert(std::abs(v0.Length() - 540.0f) < 1.0f, "Throw speed 540 at pitch 0");

    // Looking up 45°: (90+45)*6 = 810 → clamped to 750
    Vec3 vUp = GrenadeSystem::ThrowVelocity({-45, 0, 0}, {});
    Assert(std::abs(vUp.Length() - 750.0f) < 1.0f, "Throw speed clamps at 750");

    // Thrower velocity adds
    Vec3 vMov = GrenadeSystem::ThrowVelocity({0, 0, 0}, {100, 0, 0});
    Assert(vMov.Length() > v0.Length(), "Thrower velocity adds to throw");
}

static void TestGravityArc() {
    GrenadeSystem sys;
    sys.Throw(GrenadeType::HE, {0, 0, 100}, {200, 0, 0}, 0);

    std::vector<GrenadeExplosion> ex;
    float dt = 1.0f / 64.0f;

    // After 0.5s with half gravity (400 u/s²): dz = -0.5*400*0.25 = -50
    for (int i = 0; i < 32; i++)
        sys.Update(dt, 800.0f, OpenTrace, ex);

    Assert(sys.Active().size() == 1, "Grenade still in flight");
    const Grenade& g = sys.Active()[0];
    printf("  Grenade z after 0.5s: %.1f (expected ~50)\n", g.origin.z);
    Assert(g.origin.z < 60 && g.origin.z > 40,
           "Grenade falls with half gravity (~50 units in 0.5s)");
    Assert(g.origin.x > 90, "Grenade travels forward");
}

static void TestBounce() {
    GrenadeSystem sys;
    // Drop straight down onto floor
    sys.Throw(GrenadeType::HE, {0, 0, 50}, {0, 0, -300}, 0);

    std::vector<GrenadeExplosion> ex;
    float dt = 1.0f / 64.0f;

    int bounces = 0;
    for (int i = 0; i < 128 && !sys.Active().empty(); i++) {
        sys.Update(dt, 800.0f, FloorTrace, ex);
        if (!sys.Active().empty())
            bounces = sys.Active()[0].bounceCount;
    }

    printf("  Bounce count before detonation/rest: %d\n", bounces);
    Assert(bounces >= 1, "Grenade bounces off floor");
}

static void TestHEFuse() {
    GrenadeSystem sys;
    sys.Throw(GrenadeType::HE, {0, 0, 1000}, {}, 7);

    std::vector<GrenadeExplosion> ex;
    float dt = 1.0f / 64.0f;
    float t = 0;
    while (ex.empty() && t < 10.0f) {
        sys.Update(dt, 0.0f, OpenTrace, ex); // zero gravity for pure timer test
        t += dt;
    }

    printf("  HE detonated at t=%.2fs (fuse %.1fs)\n", t, GrenadeParams::HE_FUSE);
    Assert(!ex.empty(), "HE detonates");
    Assert(std::abs(t - GrenadeParams::HE_FUSE) < 0.05f, "HE fuse = 3.0s");
    Assert(ex[0].ownerId == 7, "Explosion records owner");
    Assert(sys.Active().empty(), "Detonated grenade removed");
}

static void TestSmokeDetonatesAtRest() {
    GrenadeSystem sys;
    sys.Throw(GrenadeType::Smoke, {0, 0, 30}, {0, 0, -100}, 0);

    std::vector<GrenadeExplosion> ex;
    float dt = 1.0f / 64.0f;
    for (int i = 0; i < 640 && ex.empty(); i++)
        sys.Update(dt, 800.0f, FloorTrace, ex);

    Assert(!ex.empty(), "Smoke detonates after coming to rest");
    Assert(ex[0].type == GrenadeType::Smoke, "Explosion type is smoke");
}

static void TestHEDamageFalloff() {
    float dCenter = GrenadeSystem::HEDamage(0);
    float dMid    = GrenadeSystem::HEDamage(GrenadeParams::HE_RADIUS * 0.5f);
    float dEdge   = GrenadeSystem::HEDamage(GrenadeParams::HE_RADIUS);
    float dBeyond = GrenadeSystem::HEDamage(GrenadeParams::HE_RADIUS + 1);

    printf("  HE damage: %.0f center, %.0f mid, %.0f edge\n", dCenter, dMid, dEdge);
    Assert(dCenter == GrenadeParams::HE_MAX_DAMAGE, "Max damage at center");
    Assert(std::abs(dMid - GrenadeParams::HE_MAX_DAMAGE * 0.5f) < 0.01f,
           "Half damage at half radius");
    Assert(dEdge == 0,   "Zero damage at radius edge");
    Assert(dBeyond == 0, "Zero damage beyond radius");
}

int main() {
    printf("=== Grenade Physics Tests ===\n\n");

    TestThrowVelocity();
    TestGravityArc();
    TestBounce();
    TestHEFuse();
    TestSmokeDetonatesAtRest();
    TestHEDamageFalloff();

    printf("\nAll grenade tests passed.\n");
    return 0;
}
