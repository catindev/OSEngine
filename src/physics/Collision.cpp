#include "Collision.h"
#include <cmath>
#include <algorithm>

namespace OS {

// Select GoldSrc hull by AABB size
int CollisionSystem::SelectHull(Vec3 hullMins, Vec3 hullMaxs) const {
    Vec3 size = hullMaxs - hullMins;

    if (size.x <= 8.0f)  return 0; // point hull
    if (size.x <= 32.0f) return 1; // small (player standing)
    if (size.x <= 64.0f) return 2; // medium (player crouching → wrong, but close)
    return 3;                        // large
}

int CollisionSystem::PointContents(Vec3 point) const {
    if (m_bsp.models.empty()) return CONTENTS_EMPTY;

    // Traverse node tree of model 0 (world)
    int headNode = m_bsp.models[0].headnode[0];
    int idx = headNode;
    while (idx >= 0) {
        if (idx >= (int)m_bsp.nodes.size()) return CONTENTS_EMPTY;
        const BSPNode& node = m_bsp.nodes[idx];
        float d = node.plane.DistanceTo(point);
        idx = node.children[d < 0 ? 1 : 0];
    }
    // idx is -(leaf+1)
    int leaf = -idx - 1;
    if (leaf < 0 || leaf >= (int)m_bsp.leaves.size()) return CONTENTS_EMPTY;
    return m_bsp.leaves[leaf].contents;
}

// ─── Hull trace ───────────────────────────────────────────────────────────────

int CollisionSystem::HullPointContents(int headNode, Vec3 point) const {
    const std::vector<BSPClipNode>& clipNodes = m_bsp.clipNodes;
    int num = headNode;
    while (num >= 0) {
        if (num >= (int)clipNodes.size()) return CONTENTS_SOLID;
        const BSPClipNode& node = clipNodes[num];
        float d = node.plane.normal.Dot(point) - node.plane.dist;
        num = node.children[d < 0 ? 1 : 0];
    }
    return num; // negative CONTENTS_* value
}

// Faithful port of Quake's SV_RecursiveHullCheck.
// Traces a point (not a box — the hull is already expanded) from p1 to p2.
// Returns true if the path is clear; returns false and fills ctx.result on hit.
bool CollisionSystem::RecursiveHullCheck(ClipContext& ctx,
                                          int num,
                                          float p1f, float p2f,
                                          Vec3 p1, Vec3 p2) const {
    static constexpr float DIST_EPSILON = 0.03125f;
    const std::vector<BSPClipNode>& clipNodes = m_bsp.clipNodes;

    // Leaf — determine contents.
    if (num < 0) {
        if (num != CONTENTS_SOLID) {
            ctx.result.allSolid  = false;
            if (num == CONTENTS_EMPTY)
                ctx.result.hit = false;
        } else {
            ctx.result.startSolid = true;
        }
        return true; // not a blocking collision
    }

    if (num >= (int)clipNodes.size()) return true;

    const BSPClipNode& node = clipNodes[num];
    float t1 = node.plane.normal.Dot(p1) - node.plane.dist;
    float t2 = node.plane.normal.Dot(p2) - node.plane.dist;

    // Both endpoints on the same side — descend that child.
    if (t1 >= 0 && t2 >= 0)
        return RecursiveHullCheck(ctx, node.children[0], p1f, p2f, p1, p2);
    if (t1 < 0 && t2 < 0)
        return RecursiveHullCheck(ctx, node.children[1], p1f, p2f, p1, p2);

    // Straddles — compute split fraction and midpoint.
    float frac;
    int side;
    if (t1 < 0) {
        frac = (t1 + DIST_EPSILON) / (t1 - t2);
        side = 1;
    } else {
        frac = (t1 - DIST_EPSILON) / (t1 - t2);
        side = 0;
    }
    frac = Clamp(frac, 0.0f, 1.0f);

    float midf = p1f + (p2f - p1f) * frac;
    Vec3  mid  = p1 + (p2 - p1) * frac;

    // Check the near side first.
    if (!RecursiveHullCheck(ctx, node.children[side], p1f, midf, p1, mid))
        return false;

    // Is the far side solid? If so, this is the impact.
    if (HullPointContents(ctx.headNode, mid) != CONTENTS_SOLID) {
        // Far side is passable — continue.
        return RecursiveHullCheck(ctx, node.children[side ^ 1], midf, p2f, mid, p2);
    }

    if (ctx.result.allSolid)
        return false; // started in solid, nothing to report

    // Record the impact.
    // If we entered from the back, flip the plane normal.
    ctx.result.normal   = (side == 0) ? node.plane.normal : -node.plane.normal;
    ctx.result.fraction = midf;
    ctx.result.endPos   = mid;
    ctx.result.hit      = true;
    return false;
}

TraceResult CollisionSystem::Trace(Vec3 start, Vec3 end,
                                    Vec3 hullMins, Vec3 hullMaxs,
                                    int /*contentsMask*/) const {
    ClipContext ctx;
    ctx.start    = start;
    ctx.end      = end;
    ctx.hullMins = hullMins;
    ctx.hullMaxs = hullMaxs;
    ctx.result.endPos   = end;
    ctx.result.fraction = 1.0f;
    ctx.result.allSolid = true;  // cleared once we pass any non-solid leaf
    ctx.result.hit      = false;

    if (m_bsp.models.empty() || m_bsp.clipNodes.empty()) {
        ctx.result.allSolid = false;
        return ctx.result;
    }

    int hull = SelectHull(hullMins, hullMaxs);
    ctx.headNode = m_bsp.models[0].headnode[std::min(hull, BSP_MAX_HULLS-1)];

    RecursiveHullCheck(ctx, ctx.headNode, 0.0f, 1.0f, start, end);

    if (ctx.result.startSolid) {
        ctx.result.fraction = 0.0f;
        ctx.result.endPos   = start;
        ctx.result.hit      = true;
        ctx.result.allSolid = ctx.result.allSolid; // preserve
    } else if (ctx.result.fraction < 1.0f) {
        ctx.result.endPos = start + (end - start) * ctx.result.fraction;
        ctx.result.hit    = true;
    } else {
        ctx.result.allSolid = false;
        ctx.result.endPos   = end;
    }

    return ctx.result;
}

TraceFn MakeTraceFn(const CollisionSystem& cs) {
    return [&cs](Vec3 start, Vec3 end, Vec3 hmins, Vec3 hmaxs, int mask) {
        return cs.Trace(start, end, hmins, hmaxs, mask);
    };
}

} // namespace OS
