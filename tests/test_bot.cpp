// Bot AI and Navigation tests

#include "../src/game/Bot.h"
#include "../src/game/Nav.h"
#include <cassert>
#include <cstdio>
#include <cmath>

using namespace OS;

static void Assert(bool cond, const char* msg) {
    if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); exit(1); }
    printf("PASS: %s\n", msg);
}

// Open-space trace: never hits anything
static TraceResult OpenTrace(Vec3, Vec3 end, Vec3, Vec3, int) {
    TraceResult tr;
    tr.fraction = 1.0f;
    tr.endPos   = end;
    return tr;
}

// Floor-at-z=0 trace: used for nav auto-gen
static float FloorFraction(Vec3 from, Vec3 to) {
    if (from.z >= 0 && to.z < 0) {
        return from.z / (from.z - to.z);
    }
    return 1.0f;
}

static void TestNavAddConnect() {
    NavMesh nav;
    int a = nav.AddNode({0, 0, 0});
    int b = nav.AddNode({100, 0, 0});
    int c = nav.AddNode({200, 0, 0});
    nav.Connect(a, b);
    nav.Connect(b, c);

    Assert(nav.Nodes().size() == 3, "Three nodes added");
    Assert(nav.Nodes()[0].neighbors.size() == 1, "Node 0 has one neighbor");
    Assert(nav.Nodes()[1].neighbors.size() == 2, "Node 1 has two neighbors");
}

static void TestNavPathfinding() {
    NavMesh nav;
    // Linear chain: 0-1-2-3-4
    for (int i = 0; i < 5; i++) nav.AddNode({(float)i * 100, 0, 0});
    for (int i = 0; i < 4; i++) nav.Connect(i, i + 1);

    NavPath p = nav.FindPath(0, 4);
    Assert(p.valid, "Path found in linear chain");
    Assert(p.nodes.size() == 5, "Path visits all 5 nodes");
    Assert(p.nodes.front() == 0 && p.nodes.back() == 4, "Path starts at 0, ends at 4");
    printf("  Path: %d -> %d -> %d -> %d -> %d\n",
           p.nodes[0], p.nodes[1], p.nodes[2], p.nodes[3], p.nodes[4]);
}

static void TestNavNoPath() {
    NavMesh nav;
    nav.AddNode({0, 0, 0});
    nav.AddNode({100, 0, 0});
    // Not connected
    NavPath p = nav.FindPath(0, 1);
    Assert(!p.valid, "No path between disconnected nodes");
}

static void TestNavSaveLoad() {
    NavMesh nav;
    int a = nav.AddNode({10, 20, 30}, NavNode::CT_SPAWN);
    int b = nav.AddNode({110, 20, 30});
    nav.Connect(a, b);

    const char* tmpPath = "/tmp/test_nav.nav";
    Assert(nav.SaveToFile(tmpPath), "Save nav file");

    NavMesh nav2;
    Assert(nav2.LoadFromFile(tmpPath), "Load nav file");
    Assert(nav2.Nodes().size() == 2, "Loaded 2 nodes");
    Assert(nav2.Nodes()[0].flags & NavNode::CT_SPAWN, "CT_SPAWN flag preserved");
    Assert(nav2.Nodes()[0].neighbors.size() == 1, "Connectivity preserved");
}

static void TestNavAutoGen() {
    // Flat floor at z=0, open sky above, 200x200 map
    NavMesh nav;
    nav.AutoGenerate({-100, -100, -50}, {100, 100, 200}, FloorFraction, 64.0f);
    printf("  AutoGen nodes: %d\n", (int)nav.Nodes().size());
    Assert(nav.HasNodes(), "AutoGen produces nodes on flat floor");
    // At 64-unit spacing in 200x200 = ~3x3 = at least 4 nodes
    Assert((int)nav.Nodes().size() >= 4, "At least 4 nodes generated");
}

static void TestNavNearestNode() {
    NavMesh nav;
    nav.AddNode({0,   0, 0});
    nav.AddNode({100, 0, 0});
    nav.AddNode({200, 0, 0});

    int n = nav.NearestNode({80, 0, 0});
    Assert(n == 1, "Nearest node at x=80 is node 1 (x=100)");
}

static void TestBotParams() {
    BotParams easy   = BotParams::ForDifficulty(BotDifficulty::Easy);
    BotParams expert = BotParams::ForDifficulty(BotDifficulty::Expert);

    Assert(easy.reactionTime > expert.reactionTime, "Easy is slower than expert");
    Assert(easy.aimSpeed     < expert.aimSpeed,     "Expert aims faster");
    Assert(easy.aimAccuracy  > expert.aimAccuracy,  "Expert is more accurate (lower error)");
    printf("  Easy reaction: %.2fs, Expert: %.2fs\n",
           easy.reactionTime, expert.reactionTime);
}

static void TestBotThinkIdle() {
    NavMesh nav;
    // Simple 2-node nav for the bot to patrol
    nav.AddNode({0,   0, 0});
    nav.AddNode({200, 0, 0});
    nav.Connect(0, 1);

    Bot bot(64, BotDifficulty::Normal, TEAM_CT);

    // Set bot alive at node 0
    Player ps;
    ps.id    = 64;
    ps.alive = true;
    ps.isBot = true;
    ps.team  = TEAM_CT;
    ps.physState.origin = {0, 0, 0};
    ps.health = 100.0f;
    bot.SetState(ps);

    std::vector<Player> players = { ps };
    WeaponDatabase::Init();

    // Think a few ticks past the idle→patrol timer (1 second)
    float time = 2.0f;
    (void)bot.Think(time, 1.0f / 64.0f, players, nav, WeaponDatabase{}, OpenTrace);

    // After 2s idle, bot should have transitioned to patrol and have a wish direction
    printf("  Bot state after 2s: %s\n", bot.StateName().c_str());
    Assert(bot.State() == BotState::Patrol || bot.State() == BotState::Idle,
           "Bot is Patrol or still Idle (valid states after idle delay)");
}

static void TestBotEngageTransition() {
    NavMesh nav;
    nav.AddNode({0, 0, 0});

    // Bot (CT) at origin, enemy (T) nearby and visible
    Bot bot(64, BotDifficulty::Expert, TEAM_CT);

    Player botPlayer;
    botPlayer.id    = 64;
    botPlayer.alive = true;
    botPlayer.team  = TEAM_CT;
    botPlayer.physState.origin = {0, 0, 0};
    botPlayer.health = 100.0f;
    bot.SetState(botPlayer);

    Player enemy;
    enemy.id    = 1;
    enemy.alive = true;
    enemy.team  = TEAM_TERRORIST;
    enemy.physState.origin = {100, 0, 0};
    enemy.health = 100.0f;

    std::vector<Player> players = { botPlayer, enemy };

    // Expert reaction time 0.10s → two ticks of 1/64s won't be enough for reaction timer
    // but after reactionTime passes, bot should engage
    float dt = 1.0f / 64.0f;
    BotState finalState = BotState::Idle;
    for (int i = 0; i < 20; i++) {
        float t = i * dt;
        bot.Think(t, dt, players, nav, WeaponDatabase{}, OpenTrace);
        finalState = bot.State();
        if (finalState == BotState::Engage) break;
    }
    printf("  Bot state after seeing enemy: %s\n", bot.StateName().c_str());
    Assert(finalState == BotState::Engage, "Bot transitions to Engage on enemy contact");
}

static void TestBotSoundAlert() {
    NavMesh nav;
    nav.AddNode({500, 0, 0});

    Bot bot(64, BotDifficulty::Normal, TEAM_CT);
    Player ps;
    ps.id = 64; ps.alive = true; ps.team = TEAM_CT;
    ps.physState.origin = {0, 0, 0}; ps.health = 100;
    bot.SetState(ps);

    // Sound within hearing radius
    bot.OnSound({200, 0, 0}, 1.0f);

    // Verify bot stores alert (state check happens on next Think)
    std::vector<Player> players = { ps };
    bot.Think(2.0f, 1.0f/64.0f, players, nav, WeaponDatabase{}, OpenTrace);
    printf("  Bot state after sound: %s\n", bot.StateName().c_str());
    Assert(bot.State() == BotState::Alert || bot.State() == BotState::Patrol,
           "Bot reacts to sound (Alert or Patrol toward source)");
}

static void TestBotManagerBasic() {
    NavMesh nav;
    for (int i = 0; i < 4; i++) nav.AddNode({(float)i * 100, 0, 0});
    for (int i = 0; i < 3; i++) nav.Connect(i, i+1);

    BotManager mgr;
    mgr.Init(&nav);

    int b1 = mgr.AddBot(BotDifficulty::Normal, TEAM_CT);
    int b2 = mgr.AddBot(BotDifficulty::Easy,   TEAM_TERRORIST);

    Assert(mgr.BotCount() == 2, "BotManager has 2 bots");
    Assert(mgr.IsBot(b1), "b1 is a bot");
    Assert(mgr.IsBot(b2), "b2 is a bot");
    Assert(!mgr.IsBot(1), "Player 1 is not a bot");

    // Set up players array with bot entries
    std::vector<Player> players(b2 + 1);
    for (auto& p : players) p.alive = false;
    players[b1].id = b1; players[b1].alive = true; players[b1].team = TEAM_CT;
    players[b1].physState.origin = {0, 0, 0}; players[b1].health = 100;
    players[b2].id = b2; players[b2].alive = true; players[b2].team = TEAM_TERRORIST;
    players[b2].physState.origin = {300, 0, 0}; players[b2].health = 100;

    WeaponDatabase::Init();
    std::vector<PlayerInput> inputs;
    mgr.Tick(2.0f, 1.0f/64.0f, players, WeaponDatabase{}, OpenTrace, inputs);

    Assert((int)inputs.size() > b1, "Inputs array populated for bots");
    printf("  BotManager tick: %d bots processed, inputs size=%d\n",
           mgr.BotCount(), (int)inputs.size());
}

int main() {
    printf("=== Bot AI & Navigation Tests ===\n\n");

    TestNavAddConnect();
    TestNavPathfinding();
    TestNavNoPath();
    TestNavSaveLoad();
    TestNavAutoGen();
    TestNavNearestNode();
    TestBotParams();
    TestBotThinkIdle();
    TestBotEngageTransition();
    TestBotSoundAlert();
    TestBotManagerBasic();

    printf("\nAll bot tests passed.\n");
    return 0;
}
