#pragma once
#include <cmath>
#include <cstring>
#include <algorithm>

namespace OS {

static constexpr float kPi      = 3.14159265358979323846f;
static constexpr float kTwoPi   = kPi * 2.0f;
static constexpr float kHalfPi  = kPi * 0.5f;
static constexpr float kDegToRad = kPi / 180.0f;
static constexpr float kRadToDeg = 180.0f / kPi;
static constexpr float kEpsilon = 1e-6f;

inline float DegToRad(float d) { return d * kDegToRad; }
inline float RadToDeg(float r) { return r * kRadToDeg; }
inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float Lerp(float a, float b, float t)    { return a + (b - a) * t; }

// ─── Vec2 ─────────────────────────────────────────────────────────────────────

struct Vec2 {
    float x = 0, y = 0;

    Vec2() = default;
    Vec2(float x, float y) : x(x), y(y) {}

    Vec2 operator+(Vec2 o) const { return {x+o.x, y+o.y}; }
    Vec2 operator-(Vec2 o) const { return {x-o.x, y-o.y}; }
    Vec2 operator*(float s) const { return {x*s, y*s}; }
    Vec2 operator/(float s) const { return {x/s, y/s}; }
    Vec2& operator+=(Vec2 o) { x+=o.x; y+=o.y; return *this; }
    Vec2& operator-=(Vec2 o) { x-=o.x; y-=o.y; return *this; }
    Vec2& operator*=(float s) { x*=s; y*=s; return *this; }

    float Dot(Vec2 o) const { return x*o.x + y*o.y; }
    float LengthSq() const  { return x*x + y*y; }
    float Length() const    { return std::sqrt(LengthSq()); }
    Vec2  Normalized() const { float l = Length(); return l > kEpsilon ? (*this)/l : Vec2{}; }
};

inline Vec2 operator*(float s, Vec2 v) { return v * s; }

// ─── Vec3 ─────────────────────────────────────────────────────────────────────

struct Vec3 {
    float x = 0, y = 0, z = 0;

    Vec3() = default;
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    explicit Vec3(float v) : x(v), y(v), z(v) {}

    float& operator[](int i)       { return (&x)[i]; }
    float  operator[](int i) const { return (&x)[i]; }

    Vec3  operator+(Vec3 o)  const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3  operator-(Vec3 o)  const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3  operator*(float s) const { return {x*s, y*s, z*s}; }
    Vec3  operator/(float s) const { return {x/s, y/s, z/s}; }
    Vec3  operator-()        const { return {-x, -y, -z}; }
    bool  operator==(Vec3 o) const { return x==o.x && y==o.y && z==o.z; }
    bool  operator!=(Vec3 o) const { return !(*this==o); }

    Vec3& operator+=(Vec3 o)  { x+=o.x; y+=o.y; z+=o.z; return *this; }
    Vec3& operator-=(Vec3 o)  { x-=o.x; y-=o.y; z-=o.z; return *this; }
    Vec3& operator*=(float s) { x*=s; y*=s; z*=s; return *this; }
    Vec3& operator/=(float s) { x/=s; y/=s; z/=s; return *this; }

    float Dot(Vec3 o)   const { return x*o.x + y*o.y + z*o.z; }
    float LengthSq()    const { return x*x + y*y + z*z; }
    float Length()      const { return std::sqrt(LengthSq()); }
    float Length2D()    const { return std::sqrt(x*x + y*y); }
    Vec3  Normalized()  const { float l = Length(); return l > kEpsilon ? (*this)/l : Vec3{}; }

    Vec3 Cross(Vec3 o) const {
        return { y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x };
    }

    Vec3 WithZ(float nz) const { return {x, y, nz}; }
    Vec2 XY()            const { return {x, y}; }

    bool IsZero(float eps = kEpsilon) const {
        return std::abs(x)<eps && std::abs(y)<eps && std::abs(z)<eps;
    }
};

inline Vec3 operator*(float s, Vec3 v)  { return v * s; }
inline float Dot(Vec3 a, Vec3 b)        { return a.Dot(b); }
inline Vec3  Cross(Vec3 a, Vec3 b)      { return a.Cross(b); }
inline float Distance(Vec3 a, Vec3 b)   { return (b-a).Length(); }
inline Vec3  Lerp(Vec3 a, Vec3 b, float t) { return a + (b-a)*t; }

// ─── Vec4 ─────────────────────────────────────────────────────────────────────

struct Vec4 {
    float x = 0, y = 0, z = 0, w = 0;
    Vec4() = default;
    Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    Vec4(Vec3 v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}
    float& operator[](int i)       { return (&x)[i]; }
    float  operator[](int i) const { return (&x)[i]; }
    Vec3 XYZ() const { return {x,y,z}; }
};

// ─── Angles (pitch/yaw/roll in degrees, GoldSrc convention) ─────────────────
// pitch: rotation around Y (up/down),  positive = down
// yaw:   rotation around Z (left/right), positive = counterclockwise
// roll:  rotation around X (tilt)

struct Angles {
    float pitch = 0, yaw = 0, roll = 0;

    Angles() = default;
    Angles(float p, float y, float r = 0) : pitch(p), yaw(y), roll(r) {}

    // Converts angles to forward/right/up vectors (GoldSrc convention)
    void AngleVectors(Vec3* forward, Vec3* right, Vec3* up) const;

    Vec3 Forward() const;
    Vec3 Right()   const;
    Vec3 Up()      const;

    Angles Normalized() const;
};

// ─── Mat4 (column-major, OpenGL convention) ──────────────────────────────────

struct Mat4 {
    float m[16] = {};

    Mat4() { Identity(); }

    void Identity() {
        memset(m, 0, sizeof(m));
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    float& operator()(int row, int col)       { return m[col*4 + row]; }
    float  operator()(int row, int col) const { return m[col*4 + row]; }

    Mat4 operator*(const Mat4& o) const;
    Vec4 operator*(Vec4 v) const;

    static Mat4 Perspective(float fovY, float aspect, float zNear, float zFar);
    static Mat4 LookAt(Vec3 eye, Vec3 center, Vec3 up);
    static Mat4 Translate(Vec3 t);
    static Mat4 RotateX(float rad);
    static Mat4 RotateY(float rad);
    static Mat4 RotateZ(float rad);
    static Mat4 Scale(Vec3 s);
    static Mat4 Scale(float s) { return Scale({s,s,s}); }

    // GoldSrc view matrix from position + angles
    static Mat4 View(Vec3 origin, const Angles& angles);

    const float* Data() const { return m; }
};

// ─── Plane ────────────────────────────────────────────────────────────────────

struct Plane {
    Vec3  normal;
    float dist;

    Plane() = default;
    Plane(Vec3 n, float d) : normal(n), dist(d) {}
    Plane(Vec3 a, Vec3 b, Vec3 c) {
        normal = (b-a).Cross(c-a).Normalized();
        dist   = normal.Dot(a);
    }

    float DistanceTo(Vec3 p) const { return normal.Dot(p) - dist; }

    // side: >0 front, <0 back, ==0 on plane
    int Classify(Vec3 p) const {
        float d = DistanceTo(p);
        if (d >  kEpsilon) return  1;
        if (d < -kEpsilon) return -1;
        return 0;
    }
};

// ─── AABB ─────────────────────────────────────────────────────────────────────

struct AABB {
    Vec3 mins, maxs;

    AABB() = default;
    AABB(Vec3 mins, Vec3 maxs) : mins(mins), maxs(maxs) {}

    static AABB Empty() {
        return { {1e30f,1e30f,1e30f}, {-1e30f,-1e30f,-1e30f} };
    }

    void Expand(Vec3 p) {
        mins.x = std::min(mins.x, p.x);
        mins.y = std::min(mins.y, p.y);
        mins.z = std::min(mins.z, p.z);
        maxs.x = std::max(maxs.x, p.x);
        maxs.y = std::max(maxs.y, p.y);
        maxs.z = std::max(maxs.z, p.z);
    }

    Vec3   Center() const  { return (mins + maxs) * 0.5f; }
    Vec3   Size()   const  { return maxs - mins; }
    bool   Contains(Vec3 p) const {
        return p.x>=mins.x && p.x<=maxs.x &&
               p.y>=mins.y && p.y<=maxs.y &&
               p.z>=mins.z && p.z<=maxs.z;
    }
    bool   Intersects(const AABB& o) const {
        return mins.x<=o.maxs.x && maxs.x>=o.mins.x &&
               mins.y<=o.maxs.y && maxs.y>=o.mins.y &&
               mins.z<=o.maxs.z && maxs.z>=o.mins.z;
    }
    AABB   Offset(Vec3 d) const { return {mins+d, maxs+d}; }
    AABB   Inflate(float r) const { return {mins-Vec3(r), maxs+Vec3(r)}; }
};

// ─── Ray ──────────────────────────────────────────────────────────────────────

struct Ray {
    Vec3 origin, dir;

    Ray() = default;
    Ray(Vec3 o, Vec3 d) : origin(o), dir(d.Normalized()) {}

    Vec3 At(float t) const { return origin + dir * t; }

    bool IntersectsAABB(const AABB& box, float& tMin, float& tMax) const;
    bool IntersectsPlane(const Plane& plane, float& t) const;
};

} // namespace OS
