#pragma once

#include <glm/glm.hpp>

namespace engine
{
    class Camera
    {
    public:
        Camera();

        void setPerspective(float fovRadians, float aspect, float nearZ, float farZ);
        void setView(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up);

        const glm::mat4& projection() const { return m_proj; }
        const glm::mat4& view() const { return m_view; }

        // Controls
        void setPosition(const glm::vec3& p) { m_position = p; recalcViewFromAngles(); }
        void addYawPitch(float yawDelta, float pitchDelta);
        void moveLocal(const glm::vec3& delta);

    private:
        void recalcViewFromAngles();

        glm::mat4 m_proj{1.0f};
        glm::mat4 m_view{1.0f};
        glm::vec3 m_position{2.0f, 2.0f, 2.0f};
        float m_yaw = -135.0f;   // degrees
        float m_pitch = -20.0f;  // degrees
    };
}


