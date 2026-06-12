#pragma once
#include "../math/Math.h"

namespace OS {

class Camera {
public:
    Vec3   origin  = {};
    Angles angles  = {};
    float  fovY    = 90.0f;
    float  aspect  = 16.0f / 9.0f;
    float  zNear   = 4.0f;
    float  zFar    = 16384.0f;

    // Apply mouse delta (dx/dy in pixels, sens in deg/pixel)
    void ApplyMouseDelta(float dx, float dy, float sensitivity);

    // Build view and projection matrices
    Mat4 ViewMatrix()       const;
    Mat4 ProjectionMatrix() const;
    Mat4 ViewProjection()   const;

    // Forward / right / up unit vectors
    Vec3 Forward() const { return angles.Forward(); }
    Vec3 Right()   const { return angles.Right(); }
    Vec3 Up()      const { return angles.Up(); }
};

} // namespace OS
