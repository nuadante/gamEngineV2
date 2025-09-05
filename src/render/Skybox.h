#pragma once

#include <glm/glm.hpp>
#include <memory>

namespace engine
{
    class Shader;

    class Skybox
    {
    public:
        Skybox();
        ~Skybox();

        bool initialize();
        void draw(const glm::mat4& projection, const glm::mat4& view, const glm::vec3& topColor, const glm::vec3& bottomColor);
        void destroy();

    private:
        unsigned int m_vao = 0;
        unsigned int m_vbo = 0;
        std::unique_ptr<Shader> m_shader;
    };
}


