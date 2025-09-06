#include "render/ParticleSystem.h"
#include "render/Shader.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace engine
{
    ParticleSystem::ParticleSystem() = default;
    ParticleSystem::~ParticleSystem() { shutdown(); }

    bool ParticleSystem::initialize(int maxCount)
    {
        m_maxCount = maxCount;
        m_particles.resize(maxCount);
        m_gpuBuffer.resize(maxCount * 5, 0.0f);

        const char* vs = R"GLSL(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            layout (location = 1) in float aLife;
            layout (location = 2) in float aSize;
            uniform mat4 u_View;
            uniform mat4 u_Proj;
            out float vLife;
            void main(){
                vLife = aLife;
                gl_Position = u_Proj * u_View * vec4(aPos, 1.0);
                gl_PointSize = aSize;
            }
        )GLSL";
        const char* fs = R"GLSL(
            #version 330 core
            in float vLife; out vec4 FragColor;
            uniform vec3 u_Color;
            void main(){
                // soft circular point sprite
                vec2 p = gl_PointCoord * 2.0 - 1.0;
                float r2 = dot(p,p);
                if (r2 > 1.0) discard;
                float alpha = clamp(vLife, 0.0, 1.0) * (1.0 - r2);
                FragColor = vec4(u_Color, alpha);
            }
        )GLSL";
        m_shader = std::make_unique<Shader>();
        if (!m_shader->compileFromSource(vs, fs)) return false;

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, m_gpuBuffer.size() * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        // pos (3), life (1), size (1)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(4 * sizeof(float)));
        glBindVertexArray(0);
        return true;
    }

    void ParticleSystem::shutdown()
    {
        if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
        if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
        m_shader.reset();
        m_particles.clear();
        m_gpuBuffer.clear();
        m_aliveCount = 0;
        m_spawnAcc = 0.0f;
    }

    void ParticleSystem::respawn(const glm::vec3& emitterPos, float lifetime, float size)
    {
        for (int i = 0; i < m_maxCount; ++i)
        {
            if (m_particles[i].life <= 0.0f)
            {
                Particle& p = m_particles[i];
                p.position = emitterPos;
                p.velocity = glm::vec3(((rand()%200)-100)/500.0f, 1.0f, ((rand()%200)-100)/500.0f);
                p.life = lifetime;
                p.size = size;
                return;
            }
        }
    }

    void ParticleSystem::update(float dt, bool emit, const glm::vec3& emitterPos, float spawnRate, float lifetime, float size, const glm::vec3& color, float gravityY, bool additiveBlend)
    {
        m_color = color; m_additive = additiveBlend;
        // spawn
        if (emit)
        {
            m_spawnAcc += spawnRate * dt;
            int count = (int)m_spawnAcc;
            m_spawnAcc -= count;
            for (int s = 0; s < count; ++s) respawn(emitterPos, lifetime, size);
        }
        // integrate
        m_aliveCount = 0;
        for (int i = 0; i < m_maxCount; ++i)
        {
            Particle& p = m_particles[i];
            if (p.life <= 0.0f) continue;
            p.life -= dt;
            if (p.life <= 0.0f) continue;
            p.velocity.y += gravityY * dt;
            p.position += p.velocity * dt;
            // pack
            int o = m_aliveCount * 5;
            m_gpuBuffer[o+0] = p.position.x;
            m_gpuBuffer[o+1] = p.position.y;
            m_gpuBuffer[o+2] = p.position.z;
            m_gpuBuffer[o+3] = p.life;
            m_gpuBuffer[o+4] = p.size;
            m_aliveCount++;
        }
        // upload
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_aliveCount * 5 * sizeof(float), m_gpuBuffer.data());
    }

    void ParticleSystem::draw(const float* proj, const float* view)
    {
        if (m_aliveCount <= 0) return;
        m_shader->bind();
        m_shader->setMat4("u_Proj", proj);
        m_shader->setMat4("u_View", view);
        m_shader->setVec3("u_Color", m_color.x, m_color.y, m_color.z);
        glEnable(GL_PROGRAM_POINT_SIZE);
        glEnable(GL_BLEND);
        if (m_additive) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        else glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glBindVertexArray(m_vao);
        glDrawArrays(GL_POINTS, 0, m_aliveCount);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        m_shader->unbind();
    }
}


