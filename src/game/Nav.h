#pragma once
#include "../math/Math.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <functional>

namespace OS {

// Navigation node (waypoint) in the nav graph
struct NavNode {
    int   id      = -1;
    Vec3  pos;                       // world-space center (standing height)
    float radius  = 32.0f;          // arrival radius

    // Connectivity
    std::vector<int> neighbors;     // ids of reachable nodes

    // Hint flags (bitmask)
    enum Flags : uint32_t {
        NONE       = 0,
        CT_SPAWN   = 1 << 0,
        T_SPAWN    = 1 << 1,
        BOMB_A     = 1 << 2,
        BOMB_B     = 1 << 3,
        HOSTAGE    = 1 << 4,
        LADDER     = 1 << 5,
        CROUCH     = 1 << 6,
    };
    uint32_t flags = NONE;
};

// Lightweight path result
struct NavPath {
    std::vector<int> nodes;   // node ids from start to goal (inclusive)
    bool valid = false;
};

// Navigation graph: loaded from .nav or auto-generated from BSP
class NavMesh {
public:
    void Clear();
    bool LoadFromFile(const std::string& path);
    bool SaveToFile(const std::string& path) const;

    // Generate nodes automatically from BSP floor sampling
    // (call after BSP is loaded; requires a trace function)
    using TraceFn = std::function<float(Vec3 from, Vec3 to)>; // returns hit fraction
    void AutoGenerate(Vec3 mapMin, Vec3 mapMax, const TraceFn& trace,
                      float nodeSpacing = 128.0f);

    // Find nearest node to a world position
    int  NearestNode(Vec3 pos) const;

    // A* pathfinding; returns empty path if no route
    NavPath FindPath(int fromNode, int toNode) const;
    NavPath FindPath(Vec3 from, Vec3 to) const;

    const std::vector<NavNode>& Nodes() const { return m_nodes; }
    bool  HasNodes() const { return !m_nodes.empty(); }

    // Node access for building / editing
    int  AddNode(Vec3 pos, uint32_t flags = NavNode::NONE);
    void Connect(int a, int b); // bidirectional

    // Collect nodes matching a flag
    std::vector<int> NodesWithFlag(uint32_t flag) const;

private:
    std::vector<NavNode> m_nodes;

    float HeuristicDist(int a, int b) const;
};

} // namespace OS
