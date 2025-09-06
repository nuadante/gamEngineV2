#pragma once

#include <vector>
#include <memory>
#include <glm/vec3.hpp>

namespace engine
{
    class Shader;

    class ParticleSystem
    {
    public:
        ParticleSystem();
        ~ParticleSystem();

        bool initialize(int maxCount);
        void shutdown();

        void update(float dt,
                    bool emit,
                    const glm::vec3& emitterPos,
                    float spawnRate,
                    float lifetime,
                    float size,
                    const glm::vec3& color,
                    float gravityY,
                    bool additiveBlend);

        void draw(const float* proj, const float* view);

    private:
        struct Particle
        {
            glm::vec3 position;
            glm::vec3 velocity;
            float life; // seconds remaining
            float size;
        };

        bool ensureGL();
        void respawn(const glm::vec3& emitterPos, float lifetime, float size);

    private:
        std::unique_ptr<Shader> m_shader;
        unsigned int m_vao = 0;
        unsigned int m_vbo = 0;
        int m_maxCount = 0;
        std::vector<Particle> m_particles;
        std::vector<float> m_gpuBuffer; // packed: pos(3), life(1), size(1)
        int m_aliveCount = 0;
        float m_spawnAcc = 0.0f;
        glm::vec3 m_color = {1,1,1};
        bool m_additive = true;
    };
}


