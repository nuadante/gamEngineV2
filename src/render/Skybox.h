#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <string>

namespace engine
{
    class Shader;

    class Skybox
    {
    public:
        Skybox();
        ~Skybox();

        bool initialize();
        bool loadCubemap(const std::vector<std::string>& faces);
        void setUseCubemap(bool enabled);
        void draw(const glm::mat4& projection, const glm::mat4& view, const glm::vec3& topColor, const glm::vec3& bottomColor);
        void destroy();

    private:
        unsigned int m_vao = 0;
        unsigned int m_vbo = 0;
        std::unique_ptr<Shader> m_gradShader;
        std::unique_ptr<Shader> m_cubemapShader;
        unsigned int m_cubemapTex = 0;
        bool m_hasCubemap = false;
        bool m_useCubemap = false;
    };
}


