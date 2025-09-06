#pragma once

#include <vector>

namespace engine
{
    class CascadedShadowMap
    {
    public:
        CascadedShadowMap() = default;
        ~CascadedShadowMap();

        bool create(int size, int cascades);
        void destroy();

        // Begin rendering into a specific cascade (0..cascades-1)
        void beginCascade(int index);
        void end(int screenWidth, int screenHeight);

        // Bind a cascade depth texture to texture unit slot
        void bindCascade(int index, int slot) const;

        int size() const { return m_size; }
        int cascades() const { return (int)m_depthTextures.size(); }
        unsigned int textureId(int index) const { return (index>=0 && index<(int)m_depthTextures.size()) ? m_depthTextures[index] : 0; }

    private:
        unsigned int m_fbo = 0;
        std::vector<unsigned int> m_depthTextures;
        int m_size = 0;
    };
}


