#include "../src/formats/Entity.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace OS;

static void Assert(bool cond, const char* msg) {
    if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); exit(1); }
    printf("PASS: %s\n", msg);
}

static void TestEmptyString() {
    auto ents = EntityParser::Parse("");
    Assert(ents.empty(), "Empty string yields no entities");
}

static void TestSingleEntity() {
    const char* text = "{ \"classname\" \"worldspawn\" \"key\" \"value\" }";
    auto ents = EntityParser::Parse(text);
    Assert(ents.size() == 1, "Single entity parsed");
    Assert(ents[0].ClassName() == "worldspawn", "Classname correct");
    Assert(ents[0].Get("key") == "value", "Key-value correct");
    Assert(ents[0].Get("missing", "default") == "default", "Missing key returns default");
}

static void TestVec3Parsing() {
    const char* text = "{ \"classname\" \"info_player_start\" \"origin\" \"128 256 -32\" }";
    auto ents = EntityParser::Parse(text);
    Assert(ents.size() == 1, "Parsed one entity");
    Vec3 o = ents[0].GetVec3("origin");
    Assert(o.x == 128.0f && o.y == 256.0f && o.z == -32.0f, "Vec3 origin parsed");
}

static void TestAnglesParsing() {
    const char* text = "{ \"classname\" \"func_door\" \"angles\" \"15 270 0\" }";
    auto ents = EntityParser::Parse(text);
    Angles a = ents[0].GetAngles("angles");
    Assert(a.pitch == 15.0f && a.yaw == 270.0f && a.roll == 0.0f, "Angles parsed");
}

static void TestMultipleEntities() {
    const char* text = R"(
{
"classname" "worldspawn"
"wad" "cstrike.wad"
}
{
"classname" "light"
"origin" "0 0 128"
"light" "300"
}
{
"classname" "func_buyzone"
"team_no" "1"
}
)";
    auto ents = EntityParser::Parse(text);
    Assert(ents.size() == 3, "Three entities parsed");
    Assert(ents[0].ClassName() == "worldspawn", "First: worldspawn");
    Assert(ents[1].ClassName() == "light",       "Second: light");
    Assert(ents[1].GetFloat("light") == 300.0f,  "Light value parsed");
    Assert(ents[2].ClassName() == "func_buyzone", "Third: func_buyzone");
    Assert(ents[2].GetInt("team_no") == 1,        "team_no parsed as int");
}

int main() {
    printf("=== Entity Parser Tests ===\n\n");
    TestEmptyString();
    TestSingleEntity();
    TestVec3Parsing();
    TestAnglesParsing();
    TestMultipleEntities();
    printf("\nAll entity tests passed.\n");
    return 0;
}
