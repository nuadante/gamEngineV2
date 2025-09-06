#include "render/Animator.h"
#include "render/Skeleton.h"

#include <glm/glm.hpp>

namespace engine
{
    bool Animator::play(const Animation* anim, bool loop)
    {
        m_anim = anim; m_loop = loop; m_time = 0.0f;
        m_palette.clear();
        return m_anim != nullptr;
    }

    void Animator::update(Skeleton& skel, float dt)
    {
        if (!m_anim) return;
        float tps = m_anim->ticksPerSecond > 0.0f ? m_anim->ticksPerSecond : 25.0f;
        float duration = m_anim->duration;
        m_time += dt * tps;
        if (m_loop && duration > 0.0f)
            m_time = fmod(m_time, duration);
        // find two frames for interpolation (simple nearest for now)
        const auto& frames = m_anim->frames;
        if (frames.empty()) return;
        int idx = 0;
        for (int i = 0; i < (int)frames.size(); ++i)
        {
            if (frames[i].time > m_time) { idx = glm::max(i-1, 0); break; }
            if (i == (int)frames.size()-1) idx = i;
        }
        const Keyframe& kf = frames[idx];
        // build palette: global = parentGlobal * local; skin = global * offset
        size_t nb = skel.bones.size();
        if (m_palette.size() != nb) m_palette.resize(nb);
        std::vector<glm::mat4> globals(nb);
        for (size_t b = 0; b < nb; ++b)
        {
            glm::mat4 local = (b < kf.localTransforms.size()) ? kf.localTransforms[b] : glm::mat4(1.0f);
            int parent = skel.bones[b].parent;
            glm::mat4 global = (parent >= 0) ? globals[parent] * local : local;
            globals[b] = global;
            m_palette[b] = global * skel.bones[b].offset;
        }
        skel.posePalette = m_palette;
    }
}


