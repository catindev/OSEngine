#include "Math.h"
#include <cmath>

namespace OS {

// GoldSrc angle convention:
//   pitch: around Y axis, positive tilts down (inverted from standard)
//   yaw:   around Z axis, positive rotates left (CCW when viewed from above)
//   roll:  around X axis

void Angles::AngleVectors(Vec3* forward, Vec3* right, Vec3* up) const {
    float sp = std::sin(DegToRad(pitch));
    float cp = std::cos(DegToRad(pitch));
    float sy = std::sin(DegToRad(yaw));
    float cy = std::cos(DegToRad(yaw));
    float sr = std::sin(DegToRad(roll));
    float cr = std::cos(DegToRad(roll));

    if (forward) {
        forward->x = cp * cy;
        forward->y = cp * sy;
        forward->z = -sp;
    }
    if (right) {
        right->x =  (sr*sp*cy + cr*-sy);
        right->y =  (sr*sp*sy + cr* cy);
        right->z =  sr*cp;
    }
    if (up) {
        up->x = (cr*sp*cy + -sr*-sy);
        up->y = (cr*sp*sy + -sr* cy);
        up->z =  cr*cp;
    }
}

Vec3 Angles::Forward() const {
    Vec3 f; AngleVectors(&f, nullptr, nullptr); return f;
}

Vec3 Angles::Right() const {
    Vec3 r; AngleVectors(nullptr, &r, nullptr); return r;
}

Vec3 Angles::Up() const {
    Vec3 u; AngleVectors(nullptr, nullptr, &u); return u;
}

Angles Angles::Normalized() const {
    auto NormAngle = [](float a) {
        a = std::fmod(a, 360.0f);
        if (a < 0) a += 360.0f;
        return a;
    };
    float p = pitch;
    while (p >  89.0f) p =  89.0f;
    while (p < -89.0f) p = -89.0f;
    return { p, NormAngle(yaw), NormAngle(roll) };
}

// ─── Mat4 ─────────────────────────────────────────────────────────────────────

Mat4 Mat4::operator*(const Mat4& o) const {
    Mat4 r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            r(i,j) = 0;
            for (int k = 0; k < 4; k++)
                r(i,j) += (*this)(i,k) * o(k,j);
        }
    return r;
}

Vec4 Mat4::operator*(Vec4 v) const {
    return {
        m[0]*v.x + m[4]*v.y + m[8] *v.z + m[12]*v.w,
        m[1]*v.x + m[5]*v.y + m[9] *v.z + m[13]*v.w,
        m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14]*v.w,
        m[3]*v.x + m[7]*v.y + m[11]*v.z + m[15]*v.w,
    };
}

Mat4 Mat4::Perspective(float fovY, float aspect, float zNear, float zFar) {
    float tanHalf = std::tan(DegToRad(fovY) * 0.5f);
    Mat4 r;
    r.m[0]  = 1.0f / (aspect * tanHalf);
    r.m[5]  = 1.0f / tanHalf;
    r.m[10] = -(zFar + zNear) / (zFar - zNear);
    r.m[11] = -1.0f;
    r.m[14] = -(2.0f * zFar * zNear) / (zFar - zNear);
    r.m[15] = 0.0f;
    return r;
}

Mat4 Mat4::LookAt(Vec3 eye, Vec3 center, Vec3 upVec) {
    Vec3 f = (center - eye).Normalized();
    Vec3 r = f.Cross(upVec).Normalized();
    Vec3 u = r.Cross(f);

    Mat4 m;
    m(0,0)= r.x; m(0,1)= r.y; m(0,2)= r.z;
    m(1,0)= u.x; m(1,1)= u.y; m(1,2)= u.z;
    m(2,0)=-f.x; m(2,1)=-f.y; m(2,2)=-f.z;
    m(0,3)=-r.Dot(eye);
    m(1,3)=-u.Dot(eye);
    m(2,3)= f.Dot(eye);
    m(3,3)= 1.0f;
    return m;
}

Mat4 Mat4::Translate(Vec3 t) {
    Mat4 m; m(0,3)=t.x; m(1,3)=t.y; m(2,3)=t.z; return m;
}

Mat4 Mat4::RotateX(float rad) {
    Mat4 m;
    m(1,1)= std::cos(rad); m(1,2)=-std::sin(rad);
    m(2,1)= std::sin(rad); m(2,2)= std::cos(rad);
    return m;
}

Mat4 Mat4::RotateY(float rad) {
    Mat4 m;
    m(0,0)= std::cos(rad); m(0,2)= std::sin(rad);
    m(2,0)=-std::sin(rad); m(2,2)= std::cos(rad);
    return m;
}

Mat4 Mat4::RotateZ(float rad) {
    Mat4 m;
    m(0,0)= std::cos(rad); m(0,1)=-std::sin(rad);
    m(1,0)= std::sin(rad); m(1,1)= std::cos(rad);
    return m;
}

Mat4 Mat4::Scale(Vec3 s) {
    Mat4 m; m(0,0)=s.x; m(1,1)=s.y; m(2,2)=s.z; return m;
}

Mat4 Mat4::View(Vec3 origin, const Angles& angles) {
    Vec3 f, r, u;
    angles.AngleVectors(&f, &r, &u);
    return LookAt(origin, origin + f, u);
}

// ─── Ray ──────────────────────────────────────────────────────────────────────

bool Ray::IntersectsAABB(const AABB& box, float& tMin, float& tMax) const {
    tMin = -1e30f; tMax = 1e30f;
    for (int i = 0; i < 3; i++) {
        float d = dir[i];
        float o = origin[i];
        float lo = box.mins[i], hi = box.maxs[i];
        if (std::abs(d) < kEpsilon) {
            if (o < lo || o > hi) return false;
        } else {
            float t1 = (lo - o) / d;
            float t2 = (hi - o) / d;
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return false;
        }
    }
    return tMax >= 0.0f;
}

bool Ray::IntersectsPlane(const Plane& plane, float& t) const {
    float denom = plane.normal.Dot(dir);
    if (std::abs(denom) < kEpsilon) return false;
    t = (plane.dist - plane.normal.Dot(origin)) / denom;
    return t >= 0.0f;
}

} // namespace OS
