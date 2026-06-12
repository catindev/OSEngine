#pragma once
// BSP collision system (hull tracing)

#include "../math/Math.h"
#include "../formats/BSP.h"
#include "../physics/Movement.h"

namespace OS {

// Recursion limit for clip node traversal
static constexpr int MAX_CLIP_PLANES = 5;

struct ClipContext {
    Vec3        start;
    Vec3        end;
    Vec3        hullMins;
    Vec3        hullMaxs;
    TraceResult result;
};

class CollisionSystem {
public:
    explicit CollisionSystem(const BSPFile& bsp) : m_bsp(bsp) {}

    // Trace a box from start to end through world geometry.
    // Returns a TraceResult.
    TraceResult Trace(Vec3 start, Vec3 end,
                      Vec3 hullMins, Vec3 hullMaxs,
                      int contentsMask = MASK_SOLID) const;

    // Point contents query
    int PointContents(Vec3 point) const;

    // Select the correct hull for given hull dimensions
    // Hull 0: point (BSP node hull)
    // Hull 1: player standing  (32x32x72 AABB)
    // Hull 2: player crouching (32x32x36 AABB)
    // Hull 3: large entity
    int SelectHull(Vec3 hullMins, Vec3 hullMaxs) const;

private:
    const BSPFile& m_bsp;

    void RecursiveHullCheck(ClipContext& ctx,
                            int nodeIndex,
                            float p1f, float p2f,
                            Vec3 p1, Vec3 p2,
                            int hull) const;

    bool HullPointContents(int headNode, Vec3 point, int hull) const;
};

// Build a TraceFn from a CollisionSystem (for use with PlayerMove)
TraceFn MakeTraceFn(const CollisionSystem& cs);

} // namespace OS
