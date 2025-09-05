#include "render/Skybox.h"
#include "render/Shader.h"

#include <glad/glad.h>
#include <stb_image.h>

namespace engine
{
    static const float SKYBOX_VERTICES[] = {
        // positions for a cube
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    Skybox::Skybox() = default;
    Skybox::~Skybox() { destroy(); }

    bool Skybox::initialize()
    {
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(SKYBOX_VERTICES), SKYBOX_VERTICES, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glBindVertexArray(0);

        // Gradient shader
        m_gradShader = std::make_unique<Shader>();
        const char* gradVS = R"GLSL(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            uniform mat4 u_Proj;
            uniform mat4 u_View;
            out vec3 vDir;
            void main()
            {
                vDir = aPos;
                vec4 pos = u_Proj * u_View * vec4(aPos, 1.0);
                gl_Position = pos.xyww; // force depth = 1.0
            }
        )GLSL";
        const char* gradFS = R"GLSL(
            #version 330 core
            in vec3 vDir;
            out vec4 FragColor;
            uniform vec3 u_TopColor;
            uniform vec3 u_BottomColor;
            void main()
            {
                float t = clamp(vDir.y * 0.5 + 0.5, 0.0, 1.0);
                vec3 col = mix(u_BottomColor, u_TopColor, t);
                FragColor = vec4(col, 1.0);
            }
        )GLSL";
        if (!m_gradShader->compileFromSource(gradVS, gradFS)) return false;

        // Cubemap shader
        m_cubemapShader = std::make_unique<Shader>();
        const char* cubeVS = gradVS;
        const char* cubeFS = R"GLSL(
            #version 330 core
            in vec3 vDir;
            out vec4 FragColor;
            uniform samplerCube u_Cube;
            void main()
            {
                vec3 dir = normalize(vDir);
                FragColor = texture(u_Cube, dir);
            }
        )GLSL";
        if (!m_cubemapShader->compileFromSource(cubeVS, cubeFS)) return false;

        return true;
    }

    bool Skybox::loadCubemap(const std::vector<std::string>& faces)
    {
        if (faces.size() != 6) return false;
        if (m_cubemapTex == 0) glGenTextures(1, &m_cubemapTex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemapTex);
        stbi_set_flip_vertically_on_load(0);
        for (size_t i = 0; i < 6; ++i)
        {
            int w, h, n;
            unsigned char* data = stbi_load(faces[i].c_str(), &w, &h, &n, 3);
            if (!data)
            {
                glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                return false;
            }
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (int)i, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        m_hasCubemap = true;
        return true;
    }

    void Skybox::setUseCubemap(bool enabled)
    {
        m_useCubemap = enabled;
    }

    void Skybox::draw(const glm::mat4& projection, const glm::mat4& view, const glm::vec3& topColor, const glm::vec3& bottomColor)
    {
        glDepthFunc(GL_LEQUAL);
        glm::mat4 viewNoTrans = glm::mat4(glm::mat3(view));
        glBindVertexArray(m_vao);
        if (m_useCubemap && m_hasCubemap)
        {
            m_cubemapShader->bind();
            m_cubemapShader->setMat4("u_Proj", &projection[0][0]);
            m_cubemapShader->setMat4("u_View", &viewNoTrans[0][0]);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemapTex);
            m_cubemapShader->setInt("u_Cube", 0);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
            m_cubemapShader->unbind();
        }
        else
        {
            m_gradShader->bind();
            m_gradShader->setMat4("u_Proj", &projection[0][0]);
            m_gradShader->setMat4("u_View", &viewNoTrans[0][0]);
            m_gradShader->setVec3("u_TopColor", topColor.x, topColor.y, topColor.z);
            m_gradShader->setVec3("u_BottomColor", bottomColor.x, bottomColor.y, bottomColor.z);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            m_gradShader->unbind();
        }
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);
    }

    void Skybox::destroy()
    {
        if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
        if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
        if (m_cubemapTex) { glDeleteTextures(1, &m_cubemapTex); m_cubemapTex = 0; }
        m_gradShader.reset();
        m_cubemapShader.reset();
    }
}


