#pragma once

namespace engine
{
    class ShadowMap
    {
    public:
        ShadowMap() = default;
        ~ShadowMap();

        bool create(int width, int height);
        void begin();
        void end(int restoreViewportWidth, int restoreViewportHeight);
        void bindDepthTexture(int slot) const;

        inline unsigned int depthTextureId() const { return m_depthTex; }
        inline int width() const { return m_width; }
        inline int height() const { return m_height; }

        void destroy();

    private:
        unsigned int m_fbo = 0;
        unsigned int m_depthTex = 0;
        int m_width = 0;
        int m_height = 0;
    };
}


