#include "render/Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

namespace engine
{
    Camera::Camera() = default;

    void Camera::setPerspective(float fovRadians, float aspect, float nearZ, float farZ)
    {
        m_proj = glm::perspective(fovRadians, aspect, nearZ, farZ);
    }

    void Camera::setView(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up)
    {
        m_view = glm::lookAt(eye, center, up);
        m_position = eye;
    }

    void Camera::recalcViewFromAngles()
    {
        float yawRad = glm::radians(m_yaw);
        float pitchRad = glm::radians(m_pitch);
        glm::vec3 forward;
        forward.x = cosf(yawRad) * cosf(pitchRad);
        forward.y = sinf(pitchRad);
        forward.z = sinf(yawRad) * cosf(pitchRad);
        glm::vec3 center = m_position + glm::normalize(forward);
        m_view = glm::lookAt(m_position, center, {0.0f, 1.0f, 0.0f});
    }

    void Camera::addYawPitch(float yawDelta, float pitchDelta)
    {
        m_yaw += yawDelta;
        m_pitch += pitchDelta;
        if (m_pitch > 89.0f) m_pitch = 89.0f;
        if (m_pitch < -89.0f) m_pitch = -89.0f;
        recalcViewFromAngles();
    }

    void Camera::moveLocal(const glm::vec3& delta)
    {
        // derive local axes from current view
        float yawRad = glm::radians(m_yaw);
        glm::vec3 forward = glm::normalize(glm::vec3(cosf(yawRad), 0.0f, sinf(yawRad)));
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0,1,0)));
        glm::vec3 up(0,1,0);
        m_position += forward * delta.z + right * delta.x + up * delta.y;
        recalcViewFromAngles();
    }
}


