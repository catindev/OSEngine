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

void CollisionSystem::RecursiveHullCheck(ClipContext& ctx,
                                          int nodeIndex,
                                          float p1f, float p2f,
                                          Vec3 p1, Vec3 p2,
                                          int hull) const {
    if (ctx.result.fraction <= p1f) return; // already hit something closer

    // Leaf node
    if (nodeIndex < 0) {
        if (!ctx.result.startSolid) {
            if (nodeIndex == CONTENTS_SOLID) {
                ctx.result.startSolid = true;
                ctx.result.allSolid   = true;
            } else {
                if (nodeIndex == CONTENTS_EMPTY || nodeIndex == CONTENTS_CURRENT_0 ||
                    nodeIndex == CONTENTS_CURRENT_90 || nodeIndex == CONTENTS_CURRENT_180 ||
                    nodeIndex == CONTENTS_CURRENT_270) {
                    ctx.result.hit = false;
                }
            }
        }
        ctx.result.contents = nodeIndex;
        return;
    }

    // Select the appropriate clipnode tree
    const std::vector<BSPClipNode>& clipNodes = m_bsp.clipNodes;
    if (nodeIndex >= (int)clipNodes.size()) return;

    const BSPClipNode& node = clipNodes[nodeIndex];
    const Plane& plane = node.plane;

    float t1, t2, offset;

    // Separating distance for AABB vs plane
    // Expand trace box to world box for clip
    if (plane.normal.x == 1.0f) {
        t1 = p1.x - plane.dist;
        t2 = p2.x - plane.dist;
        offset = ctx.hullMaxs.x - ctx.hullMins.x;
    } else if (plane.normal.y == 1.0f) {
        t1 = p1.y - plane.dist;
        t2 = p2.y - plane.dist;
        offset = ctx.hullMaxs.y - ctx.hullMins.y;
    } else if (plane.normal.z == 1.0f) {
        t1 = p1.z - plane.dist;
        t2 = p2.z - plane.dist;
        offset = ctx.hullMaxs.z - ctx.hullMins.z;
    } else {
        t1 = plane.normal.Dot(p1) - plane.dist;
        t2 = plane.normal.Dot(p2) - plane.dist;
        offset = std::abs(ctx.hullMaxs.x * plane.normal.x)
               + std::abs(ctx.hullMaxs.y * plane.normal.y)
               + std::abs(ctx.hullMaxs.z * plane.normal.z);
    }

    // Completely on one side?
    if (t1 >= offset && t2 >= offset) {
        RecursiveHullCheck(ctx, node.children[0], p1f, p2f, p1, p2, hull);
        return;
    }
    if (t1 < -offset && t2 < -offset) {
        RecursiveHullCheck(ctx, node.children[1], p1f, p2f, p1, p2, hull);
        return;
    }

    // Crosses the plane
    if (std::abs(t1 - t2) < kEpsilon) {
        RecursiveHullCheck(ctx, node.children[0], p1f, p2f, p1, p2, hull);
        RecursiveHullCheck(ctx, node.children[1], p1f, p2f, p1, p2, hull);
        return;
    }

    int side  = (t1 < t2) ? 1 : 0;
    float frac = (t1 + (side == 0 ? offset : -offset) + kEpsilon) / (t1 - t2);
    float frac2 = (t1 + (side == 0 ? -offset : offset) - kEpsilon) / (t1 - t2);

    frac  = Clamp(frac,  0.0f, 1.0f);
    frac2 = Clamp(frac2, 0.0f, 1.0f);

    float midf = p1f + (p2f - p1f) * frac;
    Vec3 mid = p1 + (p2 - p1) * frac;

    RecursiveHullCheck(ctx, node.children[side], p1f, midf, p1, mid, hull);

    float midf2 = p1f + (p2f - p1f) * frac2;
    Vec3 mid2 = p1 + (p2 - p1) * frac2;

    RecursiveHullCheck(ctx, node.children[1-side], midf2, p2f, mid2, p2, hull);

    // If hit on near side, record the collision
    if (ctx.result.fraction > midf && node.children[side] == CONTENTS_SOLID) {
        ctx.result.fraction = midf;
        ctx.result.normal   = (side == 0) ? plane.normal : -plane.normal;
        ctx.result.hit      = true;
        ctx.result.endPos   = p1 + (p2 - p1) * midf;
    }
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
    ctx.result.hit      = false;

    if (m_bsp.models.empty()) return ctx.result;

    int hull = SelectHull(hullMins, hullMaxs);
    int headNode = m_bsp.models[0].headnode[std::min(hull, BSP_MAX_HULLS-1)];

    RecursiveHullCheck(ctx, headNode, 0.0f, 1.0f, start, end, hull);

    if (ctx.result.startSolid) {
        ctx.result.allSolid = true;
        ctx.result.fraction = 0.0f;
        ctx.result.endPos   = start;
        ctx.result.hit      = true;
    } else if (ctx.result.fraction < 1.0f) {
        ctx.result.endPos = start + (end - start) * ctx.result.fraction;
        ctx.result.hit    = true;
    }

    return ctx.result;
}

TraceFn MakeTraceFn(const CollisionSystem& cs) {
    return [&cs](Vec3 start, Vec3 end, Vec3 hmins, Vec3 hmaxs, int mask) {
        return cs.Trace(start, end, hmins, hmaxs, mask);
    };
}

} // namespace OS
