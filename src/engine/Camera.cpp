#include "Camera.h"
#include <algorithm>

namespace OS {

void Camera::ApplyMouseDelta(float dx, float dy, float sensitivity) {
    angles.yaw   -= dx * sensitivity;
    angles.pitch += dy * sensitivity;
    angles = angles.Normalized();
}

Mat4 Camera::ViewMatrix() const {
    return Mat4::View(origin, angles);
}

Mat4 Camera::ProjectionMatrix() const {
    return Mat4::Perspective(fovY, aspect, zNear, zFar);
}

Mat4 Camera::ViewProjection() const {
    return ProjectionMatrix() * ViewMatrix();
}

} // namespace OS
