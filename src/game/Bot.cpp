#include "Bot.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <cassert>

namespace OS {

// ─── BotParams ───────────────────────────────────────────────────────────────

BotParams BotParams::ForDifficulty(BotDifficulty d) {
    BotParams p;
    switch (d) {
    case BotDifficulty::Easy:
        p.reactionTime   = 0.60f;
        p.aimSpeed       = 100.0f;
        p.aimAccuracy    = 0.80f;
        p.attackDelay    = 0.25f;
        p.hearingRadius  = 800.0f;
        break;
    case BotDifficulty::Normal:
        p.reactionTime   = 0.40f;
        p.aimSpeed       = 160.0f;
        p.aimAccuracy    = 0.55f;
        p.attackDelay    = 0.15f;
        p.hearingRadius  = 1000.0f;
        break;
    case BotDifficulty::Hard:
        p.reactionTime   = 0.25f;
        p.aimSpeed       = 240.0f;
        p.aimAccuracy    = 0.30f;
        p.attackDelay    = 0.08f;
        p.hearingRadius  = 1400.0f;
        break;
    case BotDifficulty::Expert:
        p.reactionTime   = 0.10f;
        p.aimSpeed       = 400.0f;
        p.aimAccuracy    = 0.08f;
        p.attackDelay    = 0.0f;
        p.hearingRadius  = 1800.0f;
        break;
    }
    return p;
}

// ─── Bot ─────────────────────────────────────────────────────────────────────

Bot::Bot(int entityId, BotDifficulty difficulty, int team)
    : m_entityId(entityId)
    , m_team(team)
    , m_difficulty(difficulty)
    , m_params(BotParams::ForDifficulty(difficulty))
    , m_rngState((uint32_t)(entityId * 1234567u + 42u))
{
    m_player.id    = entityId;
    m_player.isBot = true;
    m_player.team  = (team == TEAM_CT) ? TEAM_CT : TEAM_TERRORIST;
}

float Bot::Rand01() {
    // xorshift32
    m_rngState ^= m_rngState << 13;
    m_rngState ^= m_rngState >> 17;
    m_rngState ^= m_rngState << 5;
    return (float)(m_rngState & 0xFFFFFF) / (float)0x1000000;
}

float Bot::Rand11() { return Rand01() * 2.0f - 1.0f; }

std::string Bot::StateName() const {
    switch (m_state) {
    case BotState::Idle:    return "Idle";
    case BotState::Patrol:  return "Patrol";
    case BotState::Alert:   return "Alert";
    case BotState::Engage:  return "Engage";
    case BotState::Retreat: return "Retreat";
    case BotState::Plant:   return "Plant";
    case BotState::Defuse:  return "Defuse";
    case BotState::Buy:     return "Buy";
    }
    return "Unknown";
}

void Bot::TransitionTo(BotState next, float time) {
    m_state      = next;
    m_stateEnter = time;
}

// ─── visibility / LOS ────────────────────────────────────────────────────────

bool Bot::CanSeePoint(Vec3 from, Vec3 to, const TraceFn& trace) const {
    // Use a simple zero-size hull ray for visibility
    TraceResult tr = trace(from, to, {0,0,0}, {0,0,0}, MASK_SOLID);
    return tr.fraction >= 0.99f;
}

// ─── enemy tracking ──────────────────────────────────────────────────────────

void Bot::UpdateEnemy(float time, const std::vector<Player>& players, const TraceFn& trace) {
    Vec3 eye = m_player.eyePosition();
    m_enemy.visible = false;

    for (const Player& p : players) {
        if (!p.alive)         continue;
        if (p.id == m_entityId) continue;
        if (p.team == m_player.team) continue; // friendly

        Vec3 targetEye = p.eyePosition();
        if (CanSeePoint(eye, targetEye, trace)) {
            m_enemy.entityId = p.id;
            m_enemy.lastPos  = p.physState.origin;
            m_enemy.lastSeen = time;
            m_enemy.visible  = true;
            break;
        }
    }

    if (!m_enemy.visible) m_reactionTimer = m_params.reactionTime;
    else m_reactionTimer = std::max(0.0f, m_reactionTimer - 0.016f);
}

// ─── navigation ──────────────────────────────────────────────────────────────

void Bot::NavigateTo(Vec3 goal, float time, const NavMesh& nav) {
    m_goalPos    = goal;
    m_hasGoal    = true;
    m_path       = nav.FindPath(m_player.physState.origin, goal);
    m_pathNode   = 0;
    m_nextPathUpd = time + m_params.pathUpdateRate;
}

void Bot::UpdatePath(float time, const NavMesh& nav) {
    if (!m_hasGoal) return;
    if (time < m_nextPathUpd && m_path.valid) return;
    NavigateTo(m_goalPos, time, nav);
}

// ─── aim ─────────────────────────────────────────────────────────────────────

void Bot::AimToward(Vec3 target, float dt) {
    Vec3 eye  = m_player.eyePosition();
    Vec3 diff = target - eye;
    float dist   = diff.Length();
    if (dist < 0.001f) return;

    // Compute ideal angles
    float yaw   = std::atan2(diff.y, diff.x) * (180.0f / 3.14159265f);
    float pitch = -std::atan2(diff.z, std::sqrt(diff.x*diff.x + diff.y*diff.y))
                    * (180.0f / 3.14159265f);

    // Move current aim toward ideal at aimSpeed deg/sec
    auto moveAngle = [&](float cur, float ideal, float maxDelta) -> float {
        float delta = ideal - cur;
        // Wrap to [-180, 180]
        while (delta >  180.0f) delta -= 360.0f;
        while (delta < -180.0f) delta += 360.0f;
        float step = std::min(std::abs(delta), maxDelta) * (delta < 0 ? -1.0f : 1.0f);
        return cur + step;
    };

    float maxStep = m_params.aimSpeed * dt;
    m_aimAngles.yaw   = moveAngle(m_aimAngles.yaw,   yaw,   maxStep);
    m_aimAngles.pitch = moveAngle(m_aimAngles.pitch, pitch,  maxStep);

    m_player.physState.angles = m_aimAngles;
}

Vec3 Bot::AddAimError(Vec3 idealAngles, float spread) const {
    // Unused param name to avoid warning — we derive spread from params directly
    (void)idealAngles;
    return Vec3(
        m_params.aimAccuracy * spread,
        m_params.aimAccuracy * spread,
        0);
}

// ─── movement input ──────────────────────────────────────────────────────────

PlayerInput Bot::BuildMovementInput(float dt) {
    PlayerInput in;

    if (!m_path.valid || m_path.nodes.empty()) return in;
    if (m_pathNode >= (int)m_path.nodes.size()) return in;

    // TODO: get from NavMesh — for now use goalPos directly as stub
    // Advance path nodes when close enough
    // (requires nav access — we pass current nodePos as goal)
    Vec3 origin = m_player.physState.origin;
    Vec3 target = m_goalPos;

    if (m_pathNode < (int)m_path.nodes.size()) {
        // We store nav-mesh positions as the node waypoints
        // (NavMesh is not passed here; positions were copied into m_goalPos
        //  by NavigateTo — use the path incrementally)
        // For full implementation we'd store Vec3 waypoints in Bot.
        // This simplified version heads straight to goalPos.
        (void)dt;
    }

    Vec3 diff = target - origin;
    diff.z    = 0;
    float dist = diff.Length();
    if (dist < 5.0f) return in;

    diff = diff * (1.0f / dist); // normalize XY

    // Rotate wish direction into body frame
    float yawRad = m_aimAngles.yaw * (3.14159265f / 180.0f);
    float cy = std::cos(yawRad), sy = std::sin(yawRad);
    in.forwardMove = diff.x * cy + diff.y * sy;
    in.sideMove    = diff.x * -sy + diff.y * cy;

    float fwdAbs = std::abs(in.forwardMove);
    float sideAbs = std::abs(in.sideMove);
    float mag = std::sqrt(fwdAbs*fwdAbs + sideAbs*sideAbs);
    if (mag > 0.001f) {
        in.forwardMove /= mag;
        in.sideMove    /= mag;
    }

    float cos_yaw = std::cos(yawRad);
    float sin_yaw = std::sin(yawRad);
    in.wishDir  = Vec3(cos_yaw * in.forwardMove - sin_yaw * in.sideMove,
                       sin_yaw * in.forwardMove + cos_yaw * in.sideMove, 0);
    in.wishSpeed = 221.0f; // default AK-47 speed; weapon-aware later

    return in;
}

// ─── main Think ──────────────────────────────────────────────────────────────

PlayerInput Bot::Think(
    float time, float dt,
    const std::vector<Player>& allPlayers,
    const NavMesh& nav,
    const WeaponDatabase& /*wdb*/,
    const TraceFn& trace)
{
    UpdateEnemy(time, allPlayers, trace);
    UpdatePath(time, nav);

    PlayerInput in;

    // ── state machine ──
    switch (m_state) {

    case BotState::Idle:
        // Enemy spotted → engage immediately
        if (m_enemy.visible && m_reactionTimer <= 0) {
            TransitionTo(BotState::Engage, time);
            break;
        }
        // Choose patrol destination from random nav node
        if (nav.HasNodes() && time > m_stateEnter + 1.0f) {
            int n = (int)(Rand01() * nav.Nodes().size());
            NavigateTo(nav.Nodes()[n].pos, time, nav);
            TransitionTo(BotState::Patrol, time);
        }
        break;

    case BotState::Patrol:
        in = BuildMovementInput(dt);
        // Arrived at goal?
        {
            Vec3 diff = m_goalPos - m_player.physState.origin;
            if (diff.Length() < 48.0f || !m_path.valid) {
                TransitionTo(BotState::Idle, time);
            }
        }
        // Enemy spotted → engage
        if (m_enemy.visible && m_reactionTimer <= 0) {
            TransitionTo(BotState::Engage, time);
        }
        // Sound alert → investigate
        if (m_hasAlert) {
            NavigateTo(m_alertPos, time, nav);
            m_hasAlert = false;
            TransitionTo(BotState::Alert, time);
        }
        break;

    case BotState::Alert:
        in = BuildMovementInput(dt);
        {
            Vec3 diff = m_alertPos - m_player.physState.origin;
            if (diff.Length() < 64.0f || time > m_stateEnter + 10.0f)
                TransitionTo(BotState::Idle, time);
        }
        if (m_enemy.visible && m_reactionTimer <= 0)
            TransitionTo(BotState::Engage, time);
        break;

    case BotState::Engage: {
        // Aim at last known enemy position
        Vec3 aimPos = m_enemy.visible ? m_enemy.lastPos + Vec3(0, 0, 54.0f)
                                      : m_enemy.lastPos;
        AimToward(aimPos, dt);

        // Fire when aimed and reaction delay done
        bool shouldFire = m_enemy.visible &&
                          m_reactionTimer <= 0 &&
                          (time - m_stateEnter) > m_params.attackDelay;
        in.wishDir   = {}; // stand still while shooting
        in.wishSpeed = 0;

        // Inject a synthetic "fire" flag via wishSpeed abuse — actual fire
        // is driven by BotManager comparing lastFire from weapon system.
        // We encode it in upMove as a flag (engine reads this).
        in.upMove = shouldFire ? 1.0f : 0.0f;

        // Strafe slightly to be harder to hit
        float strafe = std::sin(time * 3.0f) * 0.5f;
        in.sideMove  = strafe;
        in.forwardMove = -0.1f; // slight back-pedal

        float cos_yaw = std::cos(m_aimAngles.yaw * 3.14159265f / 180.0f);
        float sin_yaw = std::sin(m_aimAngles.yaw * 3.14159265f / 180.0f);
        in.wishDir  = Vec3(cos_yaw * in.forwardMove - sin_yaw * in.sideMove,
                           sin_yaw * in.forwardMove + cos_yaw * in.sideMove, 0);
        float mag = in.wishDir.Length();
        if (mag > 0.001f) { in.wishDir = in.wishDir * (1.0f / mag); in.wishSpeed = 221.0f; }

        // Enemy lost for > 3 sec → patrol
        if (!m_enemy.visible && (time - m_enemy.lastSeen) > 3.0f)
            TransitionTo(BotState::Patrol, time);

        // Low health → retreat
        if (m_player.health < 25.0f)
            TransitionTo(BotState::Retreat, time);
        break;
    }

    case BotState::Retreat: {
        // Move away from enemy
        Vec3 awayDir = m_player.physState.origin - m_enemy.lastPos;
        awayDir.z = 0;
        float len = awayDir.Length();
        if (len > 0.001f) awayDir = awayDir * (1.0f / len);

        float yawRad = m_aimAngles.yaw * 3.14159265f / 180.0f;
        float cy = std::cos(yawRad), sy = std::sin(yawRad);
        in.forwardMove = awayDir.x * cy + awayDir.y * sy;
        in.sideMove    = awayDir.x * -sy + awayDir.y * cy;
        in.wishDir     = awayDir;
        in.wishSpeed   = 221.0f;

        if (m_player.health > 40.0f || !m_enemy.visible)
            TransitionTo(BotState::Idle, time);
        break;
    }

    default:
        break;
    }

    return in;
}

void Bot::OnSound(Vec3 origin, float loudness) {
    if (!m_player.alive) return;

    Vec3 diff = origin - m_player.physState.origin;
    float dist = diff.Length();
    if (dist > m_params.hearingRadius * loudness) return;

    // Only update alert if not currently engaged
    if (m_state != BotState::Engage) {
        m_alertPos  = origin;
        m_hasAlert  = true;
        m_alertTime = 0; // will use stateEnter when transition happens
    }
}

// ─── BotManager ──────────────────────────────────────────────────────────────

void BotManager::Init(const NavMesh* nav) {
    m_nav = nav;
    m_bots.clear();
}

void BotManager::Clear() {
    m_bots.clear();
}

int BotManager::AddBot(BotDifficulty difficulty, int team) {
    int id = m_nextEntityId++;
    m_bots.emplace_back(id, difficulty, team);
    return id;
}

bool BotManager::IsBot(int entityId) const {
    for (const Bot& b : m_bots)
        if (b.EntityId() == entityId) return true;
    return false;
}

void BotManager::Tick(float time, float dt,
                      std::vector<Player>& players,
                      const WeaponDatabase& wdb,
                      const TraceFn& trace,
                      std::vector<PlayerInput>& outInputs)
{
    if (!m_nav) return;

    // Ensure outInputs is large enough
    for (Bot& bot : m_bots) {
        int id = bot.EntityId();

        // Sync bot player state from players array
        for (const Player& p : players)
            if (p.id == id) { bot.SetState(p); break; }

        if (!bot.GetPlayer().alive) continue;

        PlayerInput in = bot.Think(time, dt, players, *m_nav, wdb, trace);

        // Resize and write
        if (id >= (int)outInputs.size()) outInputs.resize(id + 1);
        outInputs[id] = in;
    }
}

void BotManager::BroadcastSound(Vec3 origin, float loudness) {
    for (Bot& bot : m_bots)
        bot.OnSound(origin, loudness);
}

} // namespace OS
