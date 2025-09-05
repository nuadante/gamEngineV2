#include "render/Skybox.h"
#include "render/Shader.h"

#include <glad/glad.h>

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

        m_shader = std::make_unique<Shader>();
        const char* vs = R"GLSL(
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
        const char* fs = R"GLSL(
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
        return m_shader->compileFromSource(vs, fs);
    }

    void Skybox::draw(const glm::mat4& projection, const glm::mat4& view, const glm::vec3& topColor, const glm::vec3& bottomColor)
    {
        glDepthFunc(GL_LEQUAL);
        glm::mat4 viewNoTrans = glm::mat4(glm::mat3(view));
        m_shader->bind();
        m_shader->setMat4("u_Proj", &projection[0][0]);
        m_shader->setMat4("u_View", &viewNoTrans[0][0]);
        m_shader->setVec3("u_TopColor", topColor.x, topColor.y, topColor.z);
        m_shader->setVec3("u_BottomColor", bottomColor.x, bottomColor.y, bottomColor.z);
        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        m_shader->unbind();
        glDepthFunc(GL_LESS);
    }

    void Skybox::destroy()
    {
        if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
        if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
        m_shader.reset();
    }
}


