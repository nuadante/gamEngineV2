#pragma once

#include <string>
#include <memory>

namespace engine
{
    class Shader;

    class IBL
    {
    public:
        IBL();
        ~IBL();

        bool createFromHDR(const std::string& hdrPath);
        void destroy();

        unsigned int envCubemap() const { return m_envCubemap; }
        unsigned int irradianceMap() const { return m_irradianceMap; }
        unsigned int prefilterMap() const { return m_prefilterMap; }
        unsigned int brdfLUT() const { return m_brdfLUT; }

        bool valid() const { return m_envCubemap != 0; }

    private:
        bool createCapture();
        void renderCube();
        void renderQuad();

    private:
        unsigned int m_captureFBO = 0;
        unsigned int m_captureRBO = 0;
        unsigned int m_cubeVAO = 0;
        unsigned int m_cubeVBO = 0;
        unsigned int m_quadVAO = 0;
        unsigned int m_quadVBO = 0;

        unsigned int m_envCubemap = 0;
        unsigned int m_irradianceMap = 0;
        unsigned int m_prefilterMap = 0;
        unsigned int m_brdfLUT = 0;

        std::unique_ptr<Shader> m_equirectShader;
        std::unique_ptr<Shader> m_irradianceShader;
        std::unique_ptr<Shader> m_prefilterShader;
        std::unique_ptr<Shader> m_brdfShader;
    };
}


