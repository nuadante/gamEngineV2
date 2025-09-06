#include "render/CascadedShadowMap.h"
#include <glad/glad.h>

namespace engine
{
    CascadedShadowMap::~CascadedShadowMap()
    {
        destroy();
    }

    bool CascadedShadowMap::create(int size, int cascades)
    {
        destroy();
        m_size = size;
        glGenFramebuffers(1, &m_fbo);
        m_depthTextures.resize(cascades, 0);
        glGenTextures(cascades, m_depthTextures.data());
        for (int i = 0; i < cascades; ++i)
        {
            glBindTexture(GL_TEXTURE_2D, m_depthTextures[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            float borderColor[] = {1.0f,1.0f,1.0f,1.0f};
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        return m_fbo != 0;
    }

    void CascadedShadowMap::destroy()
    {
        if (!m_depthTextures.empty())
        {
            glDeleteTextures((GLsizei)m_depthTextures.size(), m_depthTextures.data());
            m_depthTextures.clear();
        }
        if (m_fbo) { glDeleteFramebuffers(1, &m_fbo); m_fbo = 0; }
        m_size = 0;
    }

    void CascadedShadowMap::beginCascade(int index)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTextures[index], 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glViewport(0, 0, m_size, m_size);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    void CascadedShadowMap::end(int screenWidth, int screenHeight)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screenWidth, screenHeight);
    }

    void CascadedShadowMap::bindCascade(int index, int slot) const
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_depthTextures[index]);
    }
}


