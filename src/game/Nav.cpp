#include "Nav.h"
#include <fstream>
#include <cstdint>
#include <queue>
#include <algorithm>
#include <cmath>
#include <limits>

namespace OS {

void NavMesh::Clear() { m_nodes.clear(); }

int NavMesh::AddNode(Vec3 pos, uint32_t flags) {
    int id = (int)m_nodes.size();
    NavNode n;
    n.id    = id;
    n.pos   = pos;
    n.flags = flags;
    m_nodes.push_back(n);
    return id;
}

void NavMesh::Connect(int a, int b) {
    if (a < 0 || b < 0 || a >= (int)m_nodes.size() || b >= (int)m_nodes.size()) return;
    auto addUnique = [](std::vector<int>& v, int x) {
        if (std::find(v.begin(), v.end(), x) == v.end()) v.push_back(x);
    };
    addUnique(m_nodes[a].neighbors, b);
    addUnique(m_nodes[b].neighbors, a);
}

// Simple binary format: magic + version + nodes
static constexpr uint32_t NAV_MAGIC   = 0x4E415601; // "NAV\x01"
static constexpr uint32_t NAV_VERSION = 1;

bool NavMesh::SaveToFile(const std::string& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    auto writeU32 = [&](uint32_t v) { f.write((char*)&v, 4); };
    auto writeF32 = [&](float   v) { f.write((char*)&v, 4); };
    auto writeI32 = [&](int32_t v) { f.write((char*)&v, 4); };

    writeU32(NAV_MAGIC);
    writeU32(NAV_VERSION);
    writeU32((uint32_t)m_nodes.size());

    for (const NavNode& n : m_nodes) {
        writeI32(n.id);
        writeF32(n.pos.x); writeF32(n.pos.y); writeF32(n.pos.z);
        writeF32(n.radius);
        writeU32(n.flags);
        writeU32((uint32_t)n.neighbors.size());
        for (int nb : n.neighbors) writeI32(nb);
    }
    return true;
}

bool NavMesh::LoadFromFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    auto readU32 = [&]() { uint32_t v; f.read((char*)&v, 4); return v; };
    auto readF32 = [&]() { float   v; f.read((char*)&v, 4); return v; };
    auto readI32 = [&]() { int32_t v; f.read((char*)&v, 4); return v; };

    if (readU32() != NAV_MAGIC)   return false;
    if (readU32() != NAV_VERSION) return false;

    uint32_t count = readU32();
    m_nodes.resize(count);

    for (uint32_t i = 0; i < count; i++) {
        NavNode& n = m_nodes[i];
        n.id     = readI32();
        n.pos.x  = readF32(); n.pos.y = readF32(); n.pos.z = readF32();
        n.radius = readF32();
        n.flags  = readU32();
        uint32_t nbCount = readU32();
        n.neighbors.resize(nbCount);
        for (uint32_t j = 0; j < nbCount; j++) n.neighbors[j] = readI32();
    }
    return (bool)f;
}

void NavMesh::AutoGenerate(Vec3 mapMin, Vec3 mapMax, const TraceFn& trace,
                            float nodeSpacing) {
    m_nodes.clear();

    // Grid sample floor points across the map bounding box
    float stepX = nodeSpacing;
    float stepY = nodeSpacing;

    for (float x = mapMin.x + stepX * 0.5f; x < mapMax.x; x += stepX) {
        for (float y = mapMin.y + stepY * 0.5f; y < mapMax.y; y += stepY) {
            Vec3 from  = { x, y, mapMax.z };
            Vec3 to    = { x, y, mapMin.z };
            float frac = trace(from, to);
            if (frac >= 1.0f) continue; // open sky / no floor

            Vec3 floor = from + (to - from) * frac;
            floor.z   += 2.0f; // lift off surface

            // Reject if we can't stand here (head hits ceiling)
            Vec3 head  = { floor.x, floor.y, floor.z + 72.0f };
            float hFrac = trace(floor, head);
            if (hFrac < 0.85f) continue; // not enough headroom

            AddNode(floor);
        }
    }

    // Connect nearby nodes that have line of sight
    int n = (int)m_nodes.size();
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            Vec3 a = m_nodes[i].pos;
            Vec3 b = m_nodes[j].pos;
            float dx = b.x - a.x, dy = b.y - a.y;
            float dist2 = dx*dx + dy*dy;
            if (dist2 > nodeSpacing * nodeSpacing * 2.5f) continue;

            // Eye-level LOS check
            Vec3 eyeA = { a.x, a.y, a.z + 54.0f };
            Vec3 eyeB = { b.x, b.y, b.z + 54.0f };
            float frac = trace(eyeA, eyeB);
            if (frac >= 0.99f) Connect(i, j);
        }
    }
}

int NavMesh::NearestNode(Vec3 pos) const {
    int   best = -1;
    float bestD2 = std::numeric_limits<float>::max();
    for (const NavNode& n : m_nodes) {
        float dx = n.pos.x - pos.x, dy = n.pos.y - pos.y, dz = n.pos.z - pos.z;
        float d2 = dx*dx + dy*dy + dz*dz;
        if (d2 < bestD2) { bestD2 = d2; best = n.id; }
    }
    return best;
}

float NavMesh::HeuristicDist(int a, int b) const {
    Vec3 pa = m_nodes[a].pos;
    Vec3 pb = m_nodes[b].pos;
    float dx = pa.x - pb.x, dy = pa.y - pb.y, dz = pa.z - pb.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

NavPath NavMesh::FindPath(int start, int goal) const {
    NavPath result;
    if (start < 0 || goal < 0 || start >= (int)m_nodes.size() || goal >= (int)m_nodes.size())
        return result;
    if (start == goal) { result.nodes.push_back(start); result.valid = true; return result; }

    // A* with euclidean heuristic
    std::vector<float> g(m_nodes.size(), std::numeric_limits<float>::max());
    std::vector<int>   parent(m_nodes.size(), -1);
    std::vector<bool>  closed(m_nodes.size(), false);

    using Entry = std::pair<float, int>; // (f, id)
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;

    g[start] = 0;
    open.push({ HeuristicDist(start, goal), start });

    while (!open.empty()) {
        auto [f, cur] = open.top(); open.pop();
        if (closed[cur]) continue;
        closed[cur] = true;
        if (cur == goal) break;

        for (int nb : m_nodes[cur].neighbors) {
            if (closed[nb]) continue;
            float cost = g[cur] + HeuristicDist(cur, nb);
            if (cost < g[nb]) {
                g[nb]      = cost;
                parent[nb] = cur;
                open.push({ cost + HeuristicDist(nb, goal), nb });
            }
        }
    }

    if (parent[goal] < 0 && goal != start) return result; // unreachable

    // Reconstruct
    std::vector<int> path;
    for (int cur = goal; cur != -1; cur = parent[cur]) path.push_back(cur);
    std::reverse(path.begin(), path.end());
    result.nodes  = std::move(path);
    result.valid  = true;
    return result;
}

NavPath NavMesh::FindPath(Vec3 from, Vec3 to) const {
    return FindPath(NearestNode(from), NearestNode(to));
}

std::vector<int> NavMesh::NodesWithFlag(uint32_t flag) const {
    std::vector<int> out;
    for (const NavNode& n : m_nodes)
        if (n.flags & flag) out.push_back(n.id);
    return out;
}

} // namespace OS
