#pragma once

#include <memory>

namespace engine
{
    class Shader;

    class PostProcess
    {
    public:
        PostProcess();
        ~PostProcess();

        bool create(int width, int height);
        void destroy();
        bool ensureSize(int width, int height);

        // Bind HDR FBO and clear
        void begin(int width, int height, float r, float g, float b, float a);
        // Rebind HDR FBO without clearing (e.g., after other passes changed FBO)
        void bind(int width, int height);
        // Draw the HDR color buffer to default framebuffer with tone mapping
        void drawToScreen(int screenWidth, int screenHeight, float exposure, float gamma, bool fxaaEnabled);

        unsigned int colorTexture() const { return m_colorTex; }

    private:
        bool createQuad();
        bool createShader();

    private:
        unsigned int m_fbo = 0;
        unsigned int m_colorTex = 0; // GL_RGBA16F
        unsigned int m_depthRbo = 0;
        int m_width = 0;
        int m_height = 0;

        unsigned int m_vao = 0;
        unsigned int m_vbo = 0;
        std::unique_ptr<Shader> m_shader;
    };
}


