// BSP loader tests using synthetic data and/or real files if available.

#include "../src/formats/BSP.h"
#include "../src/formats/Entity.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace OS;

static void Assert(bool cond, const char* msg) {
    if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); exit(1); }
    printf("PASS: %s\n", msg);
}

static void TestEntityParser() {
    const char* raw = R"(
{
"classname" "worldspawn"
"wad" "cstrike/dust.wad;cstrike/de_dust.wad"
"skyname" "nuage"
}
{
"classname" "info_player_start"
"origin" "0 0 0"
"angles" "0 90 0"
}
{
"classname" "light"
"origin" "100 200 300"
"light" "300"
}
)";

    auto entities = EntityParser::Parse(raw);
    Assert(entities.size() == 3, "Parse: found 3 entities");
    Assert(entities[0].ClassName() == "worldspawn", "First entity is worldspawn");
    Assert(entities[1].ClassName() == "info_player_start", "Second is info_player_start");

    Vec3 origin = entities[1].GetVec3("origin");
    Assert(origin.x == 0 && origin.y == 0 && origin.z == 0,
           "Parsed origin vec3");

    Angles angles = entities[1].GetAngles("angles");
    Assert(angles.yaw == 90.0f, "Parsed angles yaw=90");

    float light = entities[2].GetFloat("light");
    Assert(light == 300.0f, "Parsed float value");
}

static void TestBSPHeaderRejection() {
    // Build a fake file with wrong version
    std::vector<uint8_t> fake(sizeof(BSPRawHeader), 0);
    auto* hdr = reinterpret_cast<BSPRawHeader*>(fake.data());
    hdr->version = 29; // wrong version

    // Write to temp file and try to load
    const char* tmpPath = "/tmp/test_bad_version.bsp";
    FILE* f = fopen(tmpPath, "wb");
    if (f) {
        fwrite(fake.data(), 1, fake.size(), f);
        fclose(f);
        BSPFile bsp = BSPLoader::Load(tmpPath);
        Assert(!bsp.valid, "BSP with wrong version rejected");
        Assert(!bsp.error.empty(), "BSP error message set");
        printf("  Error: %s\n", bsp.error.c_str());
    }
}

static void TestMathVec3() {
    Vec3 a{1,2,3}, b{4,5,6};
    Vec3 c = a + b;
    Assert(c.x == 5 && c.y == 7 && c.z == 9, "Vec3 addition");

    Vec3 cross = a.Cross(b);
    Assert(std::abs(cross.x - (-3.0f)) < 0.001f, "Vec3 cross X");
    Assert(std::abs(cross.y - 6.0f) < 0.001f, "Vec3 cross Y");
    Assert(std::abs(cross.z - (-3.0f)) < 0.001f, "Vec3 cross Z");

    Vec3 norm = Vec3{3,0,0}.Normalized();
    Assert(std::abs(norm.x - 1.0f) < 0.001f, "Vec3 normalize");
    Assert(std::abs(norm.Length() - 1.0f) < 0.001f, "Normalized length = 1");
}

static void TestPlane() {
    Plane p{{0,0,1}, 5.0f}; // horizontal plane at z=5

    Assert(p.DistanceTo({0,0,10}) ==  5.0f, "Plane distance above");
    Assert(p.DistanceTo({0,0, 0}) == -5.0f, "Plane distance below");
    Assert(p.Classify({0,0,10}) ==  1, "Classify front");
    Assert(p.Classify({0,0, 0}) == -1, "Classify back");
}

static void TestAABB() {
    AABB box{{-10,-10,-10}, {10,10,10}};
    Assert(box.Contains({0,0,0}),   "AABB contains center");
    Assert(!box.Contains({20,0,0}), "AABB misses outside point");

    AABB box2{{5,-10,-10},{25,10,10}};
    Assert(box.Intersects(box2),  "Overlapping AABBs intersect");

    AABB box3{{15,-10,-10},{25,10,10}};
    Assert(!box.Intersects(box3), "Non-overlapping AABBs don't intersect");
}

int main() {
    printf("=== BSP / Format Tests ===\n\n");

    TestMathVec3();
    TestPlane();
    TestAABB();
    TestEntityParser();
    TestBSPHeaderRejection();

    printf("\nAll BSP tests passed.\n");
    return 0;
}
