#pragma once

namespace engine
{
    class PointShadowMap
    {
    public:
        PointShadowMap() = default;
        ~PointShadowMap();

        bool create(int size);
        // Begin rendering into a cube face [0..5]
        void beginFace(int faceIndex);
        void end(int restoreViewportWidth, int restoreViewportHeight);
        void bindCubemap(int slot) const;

        inline unsigned int textureId() const { return m_cubemapTex; }
        inline int size() const { return m_size; }

        void destroy();

    private:
        unsigned int m_fbo = 0;
        unsigned int m_depthRbo = 0;
        unsigned int m_cubemapTex = 0; // GL_R32F color texture storing linear distance
        int m_size = 0;
    };
}


