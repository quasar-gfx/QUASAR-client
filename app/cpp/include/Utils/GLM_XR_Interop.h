#ifndef GLM_XR_INTEROP_H
#define GLM_XR_INTEROP_H

#include <openxr/openxr.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <GraphicsAPI_OpenGL_ES.h>

namespace gxi {

struct Pose {
    glm::quat orientation;
    glm::vec3 position;
};

inline glm::quat toGLM(const XrQuaternionf &quat) {
    return glm::quat(quat.w, quat.x, quat.y, quat.z);
}

inline glm::vec3 toGLM(const XrVector3f &vec) {
    return glm::vec3(vec.x, vec.y, vec.z);
}

inline Pose toGLM(const XrPosef &pose) {
    return { toGLM(pose.orientation), toGLM(pose.position) };
}

inline glm::mat4 toGlm(const XrPosef& p) {
    Pose pose = toGLM(p);
    glm::mat4 orientation = glm::mat4_cast(pose.orientation);
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), pose.position);
    return translation * orientation;
}

glm::mat4 toGLM(const XrFovf& fov, GraphicsAPI_Type graphicsApi, float nearZ = 0.1f, float farZ = 1000.0f) {
    const float tanAngleLeft = tanf(fov.angleLeft);
    const float tanAngleRight = tanf(fov.angleRight);
    const float tanAngleUp = tanf(fov.angleUp);
    const float tanAngleDown = tanf(fov.angleDown);

    const float tanAngleWidth = tanAngleRight - tanAngleLeft;
    const float tanAngleHeight = graphicsApi == VULKAN ? (tanAngleDown - tanAngleUp) : (tanAngleUp - tanAngleDown);
    const float offsetZ = (graphicsApi == OPENGL || graphicsApi == OPENGL_ES) ? nearZ : 0;

    glm::mat4 result(0.0f);

    if (farZ <= nearZ) {
        // Infinite far plane
        result[0][0] = 2.0f / tanAngleWidth;
        result[1][1] = 2.0f / tanAngleHeight;
        result[2][2] = -1.0f;
        result[2][3] = -1.0f;
        result[3][2] = -(nearZ + offsetZ);
    } else {
        // Normal projection
        result[0][0] = 2.0f / tanAngleWidth;
        result[1][1] = 2.0f / tanAngleHeight;
        result[2][2] = -(farZ + offsetZ) / (farZ - nearZ);
        result[2][3] = -1.0f;
        result[3][2] = -(farZ * (nearZ + offsetZ)) / (farZ - nearZ);
    }

    result[2][0] = (tanAngleRight + tanAngleLeft) / tanAngleWidth;
    result[2][1] = (tanAngleUp + tanAngleDown) / tanAngleHeight;

    return result;
}

}

#endif // GLM_XR_INTEROP_H
