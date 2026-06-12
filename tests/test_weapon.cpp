// Weapon system behavioral tests: fire rate, reload timing, damage falloff,
// hit groups, semi vs auto trigger behavior.

#include "../src/game/WeaponSystem.h"
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

static ShooterContext StillContext(float time) {
    ShooterContext ctx;
    ctx.eyePos     = {0, 0, 64};
    ctx.viewAngles = {0, 0, 0}; // looking +X
    ctx.velocity2D = 0;
    ctx.onGround   = true;
    ctx.time       = time;
    return ctx;
}

static void TestFireRate() {
    WeaponDatabase::Init();
    WeaponSystem sys;
    sys.SeedRandom(42);

    WeaponState ak = WeaponSystem::MakeWeapon(WEAPON_AK47);
    const WeaponDef* def = WeaponDatabase::Get(WEAPON_AK47);

    Assert(ak.clip == 30, "AK-47 clip = 30");
    Assert(ak.reserve == 90, "AK-47 reserve = 90");

    // Simulate 1 second at 64 ticks holding trigger: expect exactly fireRate shots
    std::vector<ShootTarget> none;
    int shots = 0;
    float dt = 1.0f / 64.0f;
    for (int tick = 0; tick < 64; tick++) {
        float t = tick * dt;
        sys.Update(ak, t, dt);
        FireEvent ev = sys.TryFire(ak, StillContext(t), true, tick == 0, OpenTrace, none);
        if (ev.fired) shots++;
    }

    int expected = (int)def->fireRate; // 10 rounds/sec
    printf("  AK-47 shots in 1s: %d (expected ~%d)\n", shots, expected);
    Assert(std::abs(shots - expected) <= 1, "AK-47 fire rate ~600 RPM");
    Assert(ak.clip == 30 - shots, "Clip decremented per shot");
}

static void TestSemiAutoNeedsFreshPress() {
    WeaponSystem sys;
    sys.SeedRandom(1);

    WeaponState dgl = WeaponSystem::MakeWeapon(WEAPON_DEAGLE);
    std::vector<ShootTarget> none;

    // Holding trigger without re-press: only first shot fires
    FireEvent e1 = sys.TryFire(dgl, StillContext(0.0f), true, true, OpenTrace, none);
    Assert(e1.fired, "Deagle fires on press");

    FireEvent e2 = sys.TryFire(dgl, StillContext(0.5f), true, false, OpenTrace, none);
    Assert(!e2.fired, "Deagle held trigger does not refire (semi-auto)");

    FireEvent e3 = sys.TryFire(dgl, StillContext(1.0f), true, true, OpenTrace, none);
    Assert(e3.fired, "Deagle fires again on fresh press");
}

static void TestReloadTiming() {
    WeaponSystem sys;

    WeaponState ak = WeaponSystem::MakeWeapon(WEAPON_AK47);
    const WeaponDef* def = WeaponDatabase::Get(WEAPON_AK47);
    ak.clip = 5;

    Assert(sys.StartReload(ak, 0.0f), "Reload starts");
    Assert(ak.reloading, "Reloading flag set");

    // Before reload completes: clip unchanged
    sys.Update(ak, def->reloadTime * 0.5f, 0.016f);
    Assert(ak.clip == 5, "Clip unchanged mid-reload");

    // After reload time: clip refilled
    sys.Update(ak, def->reloadTime + 0.01f, 0.016f);
    Assert(ak.clip == 30, "Clip full after reload");
    Assert(ak.reserve == 90 - 25, "Reserve reduced by refilled amount");
    Assert(!ak.reloading, "Reloading flag cleared");
}

static void TestCannotFireWhileReloading() {
    WeaponSystem sys;
    std::vector<ShootTarget> none;

    WeaponState ak = WeaponSystem::MakeWeapon(WEAPON_AK47);
    ak.clip = 5;
    sys.StartReload(ak, 0.0f);

    FireEvent ev = sys.TryFire(ak, StillContext(0.1f), true, true, OpenTrace, none);
    Assert(!ev.fired, "Cannot fire while reloading");
}

static void TestDamageFalloff() {
    const WeaponDef* ak = WeaponDatabase::Get(WEAPON_AK47);

    float d0    = WeaponSystem::FalloffDamage(ak->damage, ak->rangeModifier, 0);
    float d500  = WeaponSystem::FalloffDamage(ak->damage, ak->rangeModifier, 500);
    float d1000 = WeaponSystem::FalloffDamage(ak->damage, ak->rangeModifier, 1000);

    printf("  AK-47 damage: %0.1f @ 0u, %0.2f @ 500u, %0.2f @ 1000u\n", d0, d500, d1000);
    Assert(d0 == ak->damage, "Full damage at point blank");
    Assert(std::abs(d500 - ak->damage * ak->rangeModifier) < 0.01f,
           "Damage * rangeModifier at 500 units");
    Assert(d1000 < d500 && d500 < d0, "Damage decreases with range");
}

static void TestHitGroups() {
    Assert(HitGroupMultiplier(HITGROUP_HEAD)    == 4.0f,  "Headshot x4");
    Assert(HitGroupMultiplier(HITGROUP_STOMACH) == 1.25f, "Stomach x1.25");
    Assert(HitGroupMultiplier(HITGROUP_LEFTLEG) == 0.75f, "Leg x0.75");
}

static void TestTargetHit() {
    WeaponSystem sys;
    sys.SeedRandom(7);

    // Target straight ahead at 200 units
    ShootTarget tgt;
    tgt.id   = 5;
    tgt.body = { {196, -16, 0},  {204, 16, 64} };
    tgt.head = { {196, -8,  64}, {204, 8,  76} };

    std::vector<ShootTarget> targets{ tgt };

    WeaponState awp = WeaponSystem::MakeWeapon(WEAPON_AWP);

    // AWP standing still has tiny spread → should hit the body box
    ShooterContext ctx = StillContext(0.0f);
    ctx.eyePos = {0, 0, 32}; // aim height intersects body box

    FireEvent ev = sys.TryFire(awp, ctx, true, true, OpenTrace, targets);
    Assert(ev.fired, "AWP fires");
    Assert(!ev.hits.empty(), "AWP produces hit record");
    Assert(ev.hits[0].hitTarget, "AWP hits target box");
    Assert(ev.hits[0].targetId == 5, "Hit correct target id");
    printf("  AWP hit damage: %.1f (base 115)\n", ev.hits[0].damage);
    Assert(ev.hits[0].damage > 100.0f, "AWP body shot damage > 100");
}

static void TestShotgunPellets() {
    WeaponSystem sys;
    sys.SeedRandom(3);
    std::vector<ShootTarget> none;

    WeaponState m3 = WeaponSystem::MakeWeapon(WEAPON_M3);
    FireEvent ev = sys.TryFire(m3, StillContext(0.0f), true, true, OpenTrace, none);

    Assert(ev.fired, "M3 fires");
    Assert(ev.bulletsFired == 9, "M3 fires 9 pellets");
    Assert(m3.clip == 7, "M3 consumes 1 shell for 9 pellets");
}

static void TestWeaponSpeeds() {
    Assert(WeaponDatabase::Get(WEAPON_AK47)->maxPlayerSpeed  == 221.0f, "AK-47 speed 221");
    Assert(WeaponDatabase::Get(WEAPON_AWP)->maxPlayerSpeed   == 210.0f, "AWP speed 210");
    Assert(WeaponDatabase::Get(WEAPON_KNIFE)->maxPlayerSpeed == 250.0f, "Knife speed 250");
    Assert(WeaponDatabase::Get(WEAPON_SCOUT)->maxPlayerSpeed == 260.0f, "Scout speed 260 (faster than knife)");
}

int main() {
    printf("=== Weapon System Tests ===\n\n");

    WeaponDatabase::Init();

    TestFireRate();
    TestSemiAutoNeedsFreshPress();
    TestReloadTiming();
    TestCannotFireWhileReloading();
    TestDamageFalloff();
    TestHitGroups();
    TestTargetHit();
    TestShotgunPellets();
    TestWeaponSpeeds();

    printf("\nAll weapon tests passed.\n");
    return 0;
}
