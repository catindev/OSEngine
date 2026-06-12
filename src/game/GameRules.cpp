#include "GameRules.h"
#include "Player.h"
#include <algorithm>

namespace OS {

void GameRules::Init(float freezeTime, float roundTime, float c4Timer,
                     int startMoney, int maxMoney, int maxRounds) {
    m_startMoney          = startMoney;
    m_maxMoney            = maxMoney;
    m_state.freezeTime    = freezeTime;
    m_state.roundTime     = roundTime;
    m_state.c4Timer       = c4Timer;
    m_state.maxRounds     = maxRounds;
    m_state.phase         = GamePhase::WarmUp;
    m_state.round         = 0;
}

void GameRules::Update(float dt, std::vector<Player>& players) {
    m_state.timer -= dt;

    switch (m_state.phase) {
    case GamePhase::WarmUp:
        if (m_state.timer <= 0) {
            m_state.phase = GamePhase::FreezeTime;
            m_state.timer = m_state.freezeTime;
            OnRoundStart();
        }
        break;

    case GamePhase::FreezeTime:
        if (m_state.timer <= 0) {
            m_state.phase = GamePhase::Playing;
            m_state.timer = m_state.roundTime;
        }
        break;

    case GamePhase::Playing:
        if (m_state.bombPlanted) {
            m_state.bombTimer -= dt;
            if (m_state.bombTimer <= 0) {
                OnRoundEnd(RoundEndReason::TerroristWin_Bomb, players);
                return;
            }
        }
        if (m_state.timer <= 0) {
            if (!m_state.bombPlanted)
                OnRoundEnd(RoundEndReason::CtWin_Time, players);
        }
        CheckWinConditions(players);
        break;

    case GamePhase::RoundEnd:
        if (m_state.timer <= 0) {
            m_state.phase = GamePhase::FreezeTime;
            m_state.timer = m_state.freezeTime;
            OnRoundStart();
        }
        break;

    case GamePhase::Intermission:
        break;
    }
}

void GameRules::OnRoundStart() {
    m_state.round++;
    m_state.bombPlanted = false;
    m_state.bombTimer   = 0;
}

void GameRules::CheckWinConditions(std::vector<Player>& players) {
    if (m_state.phase != GamePhase::Playing) return;

    int aliveCT = CountAlivePlayers(players, TEAM_CT);
    int aliveT  = CountAlivePlayers(players, TEAM_TERRORIST);

    if (aliveT == 0 && !m_state.bombPlanted)
        OnRoundEnd(RoundEndReason::CtWin_Elimination, players);
    else if (aliveCT == 0)
        OnRoundEnd(RoundEndReason::TerroristWin_Elimination, players);
}

int GameRules::CountAlivePlayers(const std::vector<Player>& players, int team) const {
    int count = 0;
    for (const auto& p : players)
        if (p.team == team && p.alive) count++;
    return count;
}

void GameRules::OnPlayerKilled(Player& victim, Player* attacker,
                                bool headshot, bool wallbang) {
    (void)headshot; (void)wallbang;
    if (attacker && attacker->team != victim.team) {
        int reward = MoneyReward::KILL_REWARD;
        // Additional knife kill bonus (CS 1.6: no extra)
        GiveMoney(*attacker, reward, m_maxMoney);
        attacker->kills++;
        attacker->killsThisRound++;
    }
    victim.deaths++;
}

void GameRules::OnBombPlanted(Player& planter, Vec3 pos) {
    m_state.bombPlanted   = true;
    m_state.bombTimer     = m_state.c4Timer;
    m_state.bombPosition  = pos;
    m_state.bomberPlayerId = planter.id;
    GiveMoney(planter, MoneyReward::BOMB_PLANT_REWARD, m_maxMoney);
}

void GameRules::OnBombDefused(Player& defuser) {
    GiveMoney(defuser, MoneyReward::BOMB_DEFUSE_REWARD, m_maxMoney);
}

void GameRules::OnRoundEnd(RoundEndReason reason, std::vector<Player>& players) {
    m_state.phase = GamePhase::RoundEnd;
    m_state.timer = 5.0f; // 5 second round end delay

    bool ctWon = (reason == RoundEndReason::CtWin_Elimination  ||
                  reason == RoundEndReason::CtWin_Defused       ||
                  reason == RoundEndReason::CtWin_Rescue        ||
                  reason == RoundEndReason::CtWin_Time);

    if (ctWon) {
        m_state.ctScore++;
        m_state.ctLoseStreak = 0;
        m_state.tLoseStreak++;
        for (auto& p : players) {
            if (p.team == TEAM_CT)
                GiveMoney(p, MoneyReward::ROUND_WIN_CT, m_maxMoney);
            else if (p.team == TEAM_TERRORIST)
                GiveMoney(p, LossBonus(m_state.tLoseStreak), m_maxMoney);
        }
    } else {
        m_state.tScore++;
        m_state.tLoseStreak = 0;
        m_state.ctLoseStreak++;
        for (auto& p : players) {
            if (p.team == TEAM_TERRORIST)
                GiveMoney(p, MoneyReward::ROUND_WIN_T, m_maxMoney);
            else if (p.team == TEAM_CT)
                GiveMoney(p, LossBonus(m_state.ctLoseStreak), m_maxMoney);
        }
    }

    if (m_onRoundEnd) m_onRoundEnd(reason);
}

int GameRules::LossBonus(int streak) {
    // CS 1.6 loss bonus:  1=$1400, 2=$1900, 3=$2400, 4=$2900, 5+=$3400
    static constexpr int bonuses[] = {1400, 1900, 2400, 2900, 3400};
    int idx = std::min(streak - 1, 4);
    if (idx < 0) idx = 0;
    return bonuses[idx];
}

void GameRules::GiveMoney(Player& p, int amount, int maxMoney) {
    p.money = std::min(p.money + amount, maxMoney);
}

} // namespace OS
