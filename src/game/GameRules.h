#pragma once
// CS 1.6 game rules: round management, money economy, win conditions.

#include <vector>
#include <functional>
#include "../math/Math.h"

namespace OS {

struct Player;

enum class GamePhase {
    WarmUp,
    FreezeTime, // buy phase
    Playing,
    RoundEnd,
    Intermission,
};

enum class RoundEndReason {
    CtWin_Elimination,
    CtWin_Defused,
    CtWin_Rescue,
    CtWin_Time,
    TerroristWin_Bomb,
    TerroristWin_Elimination,
    TerroristWin_Hostage,
    Draw,
};

// CS 1.6 money rewards
struct MoneyReward {
    static constexpr int KILL_REWARD        = 300;
    static constexpr int BOMB_PLANT_REWARD  = 800;
    static constexpr int BOMB_DEFUSE_REWARD = 3500;
    static constexpr int ROUND_WIN_CT       = 3250;
    static constexpr int ROUND_WIN_T        = 3500;
    static constexpr int ROUND_LOSS_BASE    = 1400; // increases with losing streak
    static constexpr int MAX_MONEY          = 16000;
    static constexpr int START_MONEY        = 800;
};

struct GameState {
    GamePhase phase      = GamePhase::WarmUp;
    int       round      = 0;
    float     timer      = 0;  // time remaining in current phase
    float     freezeTime = 6;
    float     roundTime  = 150; // 2.5 minutes
    float     c4Timer    = 35;

    int       ctScore    = 0;
    int       tScore     = 0;
    int       maxRounds  = 30;

    bool      bombPlanted = false;
    float     bombTimer   = 0;
    Vec3      bombPosition;
    int       bomberPlayerId = -1;

    // Losing streak bonuses (1-indexed: streak[0] = first loss, etc.)
    int       ctLoseStreak = 0;
    int       tLoseStreak  = 0;
};

using RoundEndCallback = std::function<void(RoundEndReason)>;

class GameRules {
public:
    void Init(float freezeTime, float roundTime, float c4Timer,
              int startMoney, int maxMoney, int maxRounds);

    void Update(float dt, std::vector<Player>& players);

    void OnPlayerKilled(Player& victim, Player* attacker, bool headshot, bool wallbang);
    void OnBombPlanted (Player& planter, Vec3 pos);
    void OnBombDefused (Player& defuser);
    void OnRoundStart  ();
    void OnRoundEnd    (RoundEndReason reason, std::vector<Player>& players);

    void SetRoundEndCallback(RoundEndCallback cb) { m_onRoundEnd = cb; }

    const GameState& State() const { return m_state; }
    GameState&       State()       { return m_state; }

    // Economy
    static int LossBonus(int streak);
    static void GiveMoney(Player& p, int amount, int maxMoney);

private:
    GameState m_state;
    RoundEndCallback m_onRoundEnd;

    int m_startMoney = 800;
    int m_maxMoney   = 16000;

    void CheckWinConditions(std::vector<Player>& players);
    int CountAlivePlayers(const std::vector<Player>& players, int team) const;
};

} // namespace OS
