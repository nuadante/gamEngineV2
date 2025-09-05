#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace engine
{
    class Transform
    {
    public:
        glm::vec3 position{0.0f, 0.0f, 0.0f};
        glm::vec3 rotationEuler{0.0f, 0.0f, 0.0f}; // radians
        glm::vec3 scale{1.0f, 1.0f, 1.0f};

        glm::mat4 modelMatrix() const
        {
            glm::mat4 m(1.0f);
            m = glm::translate(m, position);
            m = glm::rotate(m, rotationEuler.x, glm::vec3(1.0f, 0.0f, 0.0f));
            m = glm::rotate(m, rotationEuler.y, glm::vec3(0.0f, 1.0f, 0.0f));
            m = glm::rotate(m, rotationEuler.z, glm::vec3(0.0f, 0.0f, 1.0f));
            m = glm::scale(m, scale);
            return m;
        }
    };
}


