// MDL animation decoder tests using a synthetic in-memory model.

#include "../src/formats/MDL.h"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <cstring>

using namespace OS;

static void Assert(bool cond, const char* msg) {
    if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); exit(1); }
    printf("PASS: %s\n", msg);
}

// Build a synthetic MDLFile with one bone and a 4-frame X-translation anim
static MDLFile MakeSyntheticMDL() {
    MDLFile mdl;
    mdl.valid = true;

    MDLBone bone;
    bone.name   = "root";
    bone.parent = -1;
    for (int d = 0; d < 6; d++) { bone.defValue[d] = 0; bone.scale[d] = 1.0f; }
    mdl.bones.push_back(bone);

    MDLSequence seq;
    seq.name      = "test";
    seq.fps       = 10;
    seq.numFrames = 4;

    // Raw data layout: [anim struct][X channel RLE data]
    // anim struct: uint16 offset[6]; X offset points past the struct, others 0
    // RLE: header {valid=4, total=4} then 4 values: 0, 10, 20, 30
    struct RawAnim { uint16_t offset[6]; };

    std::vector<uint8_t> raw(1024, 0);
    size_t animOfs = 64; // arbitrary offset into raw data

    RawAnim anim = {};
    anim.offset[0] = sizeof(RawAnim); // X channel data immediately after struct
    memcpy(raw.data() + animOfs, &anim, sizeof(anim));

    int16_t* vals = reinterpret_cast<int16_t*>(raw.data() + animOfs + sizeof(RawAnim));
    // animvalue header: valid=4, total=4 (packed as two bytes in int16 slot)
    uint8_t* hdr = reinterpret_cast<uint8_t*>(&vals[0]);
    hdr[0] = 4; // valid
    hdr[1] = 4; // total
    vals[1] = 0;
    vals[2] = 10;
    vals[3] = 20;
    vals[4] = 30;

    seq.animIndex = (int32_t)animOfs;
    mdl.sequences.push_back(seq);
    mdl.rawData = std::move(raw);

    return mdl;
}

static void TestDefaultPose() {
    MDLFile mdl = MakeSyntheticMDL();

    std::vector<Mat4> bones;
    MDLAnimator::ComputeBones(mdl, -1, 0, bones); // no sequence: default pose

    Assert(bones.size() == 1, "One bone transform");
    Vec3 p = bones[0].TransformPoint({0,0,0});
    Assert(p.IsZero(0.001f), "Default pose at origin");
}

static void TestAnimFrames() {
    MDLFile mdl = MakeSyntheticMDL();

    std::vector<Mat4> bones;

    MDLAnimator::ComputeBones(mdl, 0, 0, bones);
    Vec3 p0 = bones[0].TransformPoint({0,0,0});
    printf("  Frame 0: x=%.1f\n", p0.x);
    Assert(std::abs(p0.x - 0.0f) < 0.001f, "Frame 0: x=0");

    MDLAnimator::ComputeBones(mdl, 0, 1, bones);
    Vec3 p1 = bones[0].TransformPoint({0,0,0});
    printf("  Frame 1: x=%.1f\n", p1.x);
    Assert(std::abs(p1.x - 10.0f) < 0.001f, "Frame 1: x=10");

    MDLAnimator::ComputeBones(mdl, 0, 3, bones);
    Vec3 p3 = bones[0].TransformPoint({0,0,0});
    Assert(std::abs(p3.x - 30.0f) < 0.001f, "Frame 3: x=30");
}

static void TestFrameInterpolation() {
    MDLFile mdl = MakeSyntheticMDL();

    std::vector<Mat4> bones;
    MDLAnimator::ComputeBones(mdl, 0, 1.5f, bones);
    Vec3 p = bones[0].TransformPoint({0,0,0});
    printf("  Frame 1.5: x=%.1f (expected 15)\n", p.x);
    Assert(std::abs(p.x - 15.0f) < 0.01f, "Fractional frame interpolates");
}

static void TestQuatBasics() {
    // Identity quaternion: no rotation
    Quat q;
    Mat4 m = Mat4::FromQuatPos(q, {5, 6, 7});
    Vec3 p = m.TransformPoint({1, 0, 0});
    Assert(std::abs(p.x - 6.0f) < 0.001f &&
           std::abs(p.y - 6.0f) < 0.001f &&
           std::abs(p.z - 7.0f) < 0.001f, "Identity quat translates only");

    // 90° rotation around Z: +X becomes +Y
    Quat qz = Quat::FromEulerXYZ({0, 0, kHalfPi});
    Mat4 mz = Mat4::FromQuatPos(qz, {});
    Vec3 px = mz.TransformPoint({1, 0, 0});
    printf("  Rot Z 90: (1,0,0) -> (%.2f, %.2f, %.2f)\n", px.x, px.y, px.z);
    Assert(std::abs(px.x) < 0.001f && std::abs(px.y - 1.0f) < 0.001f,
           "90 deg Z rotation maps +X to +Y");

    // Slerp halfway: 45°
    Quat qHalf = Quat::Slerp(Quat{}, qz, 0.5f);
    Mat4 mh = Mat4::FromQuatPos(qHalf, {});
    Vec3 ph = mh.TransformPoint({1, 0, 0});
    Assert(std::abs(ph.x - std::sqrt(0.5f)) < 0.01f &&
           std::abs(ph.y - std::sqrt(0.5f)) < 0.01f,
           "Slerp t=0.5 gives 45 deg rotation");
}

static void TestBoneHierarchy() {
    MDLFile mdl;
    mdl.valid = true;

    // Parent at origin, child offset +10 X from parent
    MDLBone parent;
    parent.name = "parent"; parent.parent = -1;
    for (int d = 0; d < 6; d++) { parent.defValue[d] = 0; parent.scale[d] = 1; }

    MDLBone child;
    child.name = "child"; child.parent = 0;
    for (int d = 0; d < 6; d++) { child.defValue[d] = 0; child.scale[d] = 1; }
    child.defValue[0] = 10.0f; // X offset from parent

    mdl.bones.push_back(parent);
    mdl.bones.push_back(child);

    std::vector<Mat4> bones;
    MDLAnimator::ComputeBones(mdl, -1, 0, bones);

    Vec3 childPos = bones[1].TransformPoint({0,0,0});
    printf("  Child bone world pos: (%.1f, %.1f, %.1f)\n",
           childPos.x, childPos.y, childPos.z);
    Assert(std::abs(childPos.x - 10.0f) < 0.001f, "Child bone inherits parent transform");
}

int main() {
    printf("=== MDL Animation Tests ===\n\n");

    TestQuatBasics();
    TestDefaultPose();
    TestAnimFrames();
    TestFrameInterpolation();
    TestBoneHierarchy();

    printf("\nAll MDL animation tests passed.\n");
    return 0;
}
