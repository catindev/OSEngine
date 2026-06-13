#pragma once
#include "Player.h"
#include "Nav.h"
#include "WeaponSystem.h"
#include "Projectile.h"
#include "../physics/Movement.h"
#include <optional>
#include <string>

namespace OS {

// CS 1.6 bot difficulty levels
enum class BotDifficulty { Easy, Normal, Hard, Expert };

// Bot personality parameters (tuned per difficulty)
struct BotParams {
    float reactionTime    = 0.40f; // seconds before bot sees and reacts to enemy
    float aimSpeed        = 180.0f; // degrees/sec max aim adjustment
    float aimAccuracy     = 0.6f;  // 0=perfect, 1=max spread
    float attackDelay     = 0.1f;  // extra delay before pulling trigger
    float pathUpdateRate  = 0.5f;  // how often to recalculate path (seconds)
    float hearingRadius   = 1200.f;// units: can hear footsteps within this range

    static BotParams ForDifficulty(BotDifficulty d);
};

// Bot state machine states
enum class BotState {
    Idle,       // no task
    Patrol,     // walking between nodes
    Alert,      // heard something, moving to investigate
    Engage,     // active enemy contact (shooting)
    Retreat,    // low health, backing off
    Plant,      // planting bomb (T)
    Defuse,     // defusing bomb (CT)
    Buy,        // in buy zone purchasing
};

// A visible enemy record (fills from per-tick visibility check)
struct EnemyRecord {
    int    entityId  = -1;
    Vec3   lastPos;
    float  lastSeen  = -999.f; // game time when last visible
    bool   visible   = false;
};

// Bot controller: wraps a Player slot, owns nav path, and drives PlayerInput
class Bot {
public:
    Bot(int entityId, BotDifficulty difficulty, int team);

    // Called every server tick (dt = tick interval, time = server time)
    PlayerInput Think(
        float time, float dt,
        const std::vector<Player>& allPlayers,
        const NavMesh& nav,
        const WeaponDatabase& wdb,
        const TraceFn& trace);

    // Notify about a sound event in the world (e.g., shot fired, footstep)
    void OnSound(Vec3 origin, float loudness);

    // Externally set bot position/state (synced from physics step)
    void SetState(const Player& ps) { m_player = ps; }
    const Player& GetPlayer() const { return m_player; }

    int EntityId() const { return m_entityId; }
    int Team()     const { return m_team; }

    // Current bot-internal state (for HUD/debug)
    BotState     State()   const { return m_state; }
    std::string  StateName() const;

private:
    // --- think sub-systems ---
    void  UpdateEnemy(float time, const std::vector<Player>& players, const TraceFn& trace);
    void  UpdatePath(float time, const NavMesh& nav);
    void  NavigateTo(Vec3 goal, float time, const NavMesh& nav);

    PlayerInput BuildMovementInput(float dt);
    void        AimToward(Vec3 target, float dt);

    // Skill helpers
    Vec3  AddAimError(Vec3 idealAngles, float spread) const;
    bool  CanSeePoint(Vec3 from, Vec3 to, const TraceFn& trace) const;
    int   ChooseBestWeapon(const WeaponDatabase& wdb) const;

    // State transitions
    void TransitionTo(BotState next, float time);

    // --- data ---
    int           m_entityId;
    int           m_team;
    BotDifficulty m_difficulty;
    BotParams     m_params;
    Player        m_player; // current physics state

    BotState      m_state       = BotState::Idle;
    float         m_stateEnter  = 0.0f;

    // Navigation
    NavPath       m_path;
    int           m_pathNode    = 0;    // index into m_path.nodes we're heading toward
    float         m_nextPathUpd = 0.0f;
    Vec3          m_goalPos;
    bool          m_hasGoal     = false;

    // Enemy tracking
    EnemyRecord   m_enemy;
    float         m_reactionTimer = 0.0f; // counts down after spotting enemy

    // Aim state (current view angles the bot is "looking at")
    Angles        m_aimAngles;

    // Sound alert
    Vec3          m_alertPos;
    bool          m_hasAlert    = false;
    float         m_alertTime   = 0.0f;

    // Random seed for aim jitter
    uint32_t      m_rngState;
    float         Rand01();
    float         Rand11();
};

// BotManager: owns all bots, drives Think loop, merges inputs into PlayerInput array
class BotManager {
public:
    void Init(const NavMesh* nav);
    void Clear();

    // Add a bot to the game
    int  AddBot(BotDifficulty difficulty, int team);

    // Called every tick; fills outInputs[entityId] for each bot
    void Tick(float time, float dt,
              std::vector<Player>& players,
              const WeaponDatabase& wdb,
              const TraceFn& trace,
              std::vector<PlayerInput>& outInputs);

    // Broadcast a sound event to all bots within hearing range
    void BroadcastSound(Vec3 origin, float loudness);

    int  BotCount() const { return (int)m_bots.size(); }
    bool IsBot(int entityId) const;

    const NavMesh* Nav() const { return m_nav; }

private:
    const NavMesh*   m_nav = nullptr;
    std::vector<Bot> m_bots;
    int              m_nextEntityId = 64; // bots get entity ids 64+
};

} // namespace OS
