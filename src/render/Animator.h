#pragma once

#include <glm/mat4x4.hpp>
#include <vector>
#include <string>

namespace engine
{
    struct Animation;
    class Skeleton;

    class Animator
    {
    public:
        bool play(const Animation* anim, bool loop);
        void update(Skeleton& skel, float dt);
        const std::vector<glm::mat4>& palette() const { return m_palette; }

    private:
        const Animation* m_anim = nullptr;
        bool m_loop = true;
        float m_time = 0.0f; // in animation ticks
        std::vector<glm::mat4> m_palette; // cache
    };
}


