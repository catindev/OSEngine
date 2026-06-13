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

// Distance offset to keep the trace endpoint a hair off the surface.
static constexpr float DIST_EPSILON = 0.03125f; // 1/32 unit, as in Quake/GoldSrc

// Walk a clipnode hull to find the contents (a negative CONTENTS_* value) at a
// point. GoldSrc clip hulls are pre-expanded at compile time, so this is a pure
// point query — the box size is already baked into the hull planes.
int CollisionSystem::HullPointContents(int headNode, Vec3 point) const {
    const std::vector<BSPClipNode>& clipNodes = m_bsp.clipNodes;
    int num = headNode;
    while (num >= 0) {
        if (num >= (int)clipNodes.size()) return CONTENTS_SOLID;
        const BSPClipNode& node = clipNodes[num];
        float d = node.plane.normal.Dot(point) - node.plane.dist;
        num = node.children[d < 0 ? 1 : 0];
    }
    return num; // negative contents value
}

bool CollisionSystem::RecursiveHullCheck(ClipContext& ctx,
                                          int num,
                                          float p1f, float p2f,
                                          Vec3 p1, Vec3 p2) const {
    const std::vector<BSPClipNode>& clipNodes = m_bsp.clipNodes;

    // Reached a leaf (contents value).
    if (num < 0) {
        if (num != CONTENTS_SOLID) {
            ctx.result.allSolid = false;   // got at least partway through open space
        } else {
            ctx.result.startSolid = true;  // a portion of the move began in solid
        }
        return true; // not blocked here — keep going
    }

    if (num >= (int)clipNodes.size()) return true;

    const BSPClipNode& node = clipNodes[num];
    const Plane& plane = node.plane;

    float t1 = plane.normal.Dot(p1) - plane.dist;
    float t2 = plane.normal.Dot(p2) - plane.dist;

    // Both points on the same side: descend that child only.
    if (t1 >= 0 && t2 >= 0)
        return RecursiveHullCheck(ctx, node.children[0], p1f, p2f, p1, p2);
    if (t1 < 0 && t2 < 0)
        return RecursiveHullCheck(ctx, node.children[1], p1f, p2f, p1, p2);

    // The segment crosses the plane — split it, biasing the crosspoint a hair
    // onto the near side.
    float frac;
    if (t1 < 0) frac = (t1 + DIST_EPSILON) / (t1 - t2);
    else        frac = (t1 - DIST_EPSILON) / (t1 - t2);
    frac = Clamp(frac, 0.0f, 1.0f);

    float midf = p1f + (p2f - p1f) * frac;
    Vec3  mid  = p1 + (p2 - p1) * frac;

    int side = (t1 < 0) ? 1 : 0;

    // Move up to the node on the near side.
    if (!RecursiveHullCheck(ctx, node.children[side], p1f, midf, p1, mid))
        return false;

    // If the far side isn't solid, continue the trace past the plane.
    if (HullPointContents(ctx.headNode, mid) != CONTENTS_SOLID)
        if (HullPointContents(node.children[side ^ 1], mid) != CONTENTS_SOLID)
            return RecursiveHullCheck(ctx, node.children[side ^ 1], midf, p2f, mid, p2);

    // The other side is solid: this is the impact point.
    if (ctx.result.allSolid)
        return false; // never got out of solid

    // Record the impact plane (flip when the near side was the back side).
    ctx.result.normal = (side == 0) ? plane.normal : -plane.normal;

    // Back the crosspoint out of any residual solid (rarely needed).
    while (HullPointContents(ctx.headNode, mid) == CONTENTS_SOLID) {
        frac -= 0.1f;
        if (frac < 0) {
            ctx.result.fraction = midf;
            ctx.result.endPos   = mid;
            ctx.result.hit      = true;
            return false;
        }
        midf = p1f + (p2f - p1f) * frac;
        mid  = p1 + (p2 - p1) * frac;
    }

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
    ctx.result.allSolid = true;  // cleared as soon as we pass through open space
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
    } else if (ctx.result.fraction < 1.0f) {
        ctx.result.endPos = start + (end - start) * ctx.result.fraction;
        ctx.result.hit    = true;
    } else {
        ctx.result.endPos = end;
    }

    return ctx.result;
}

TraceFn MakeTraceFn(const CollisionSystem& cs) {
    return [&cs](Vec3 start, Vec3 end, Vec3 hmins, Vec3 hmaxs, int mask) {
        return cs.Trace(start, end, hmins, hmaxs, mask);
    };
}

} // namespace OS
