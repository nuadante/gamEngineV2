#include "render/ShadowMap.h"

#include <glad/glad.h>

namespace engine
{
    ShadowMap::~ShadowMap() { destroy(); }

    bool ShadowMap::create(int width, int height)
    {
        m_width = width; m_height = height;
        glGenFramebuffers(1, &m_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

        glGenTextures(1, &m_depthTex);
        glBindTexture(GL_TEXTURE_2D, m_depthTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTex, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        bool ok = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return ok;
    }

    void ShadowMap::begin()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, m_width, m_height);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    void ShadowMap::end(int restoreViewportWidth, int restoreViewportHeight)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, restoreViewportWidth, restoreViewportHeight);
    }

    void ShadowMap::bindDepthTexture(int slot) const
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_depthTex);
    }

    void ShadowMap::destroy()
    {
        if (m_depthTex)
        {
            glDeleteTextures(1, &m_depthTex);
            m_depthTex = 0;
        }
        if (m_fbo)
        {
            glDeleteFramebuffers(1, &m_fbo);
            m_fbo = 0;
        }
    }
}


