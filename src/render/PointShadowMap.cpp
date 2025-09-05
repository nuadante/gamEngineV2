#include "render/PointShadowMap.h"

#include <glad/glad.h>

namespace engine
{
    PointShadowMap::~PointShadowMap()
    {
        destroy();
    }

    bool PointShadowMap::create(int size)
    {
        m_size = size;
        glGenFramebuffers(1, &m_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

        // Color cubemap to store distance
        glGenTextures(1, &m_cubemapTex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemapTex);
        for (int i = 0; i < 6; ++i)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_R32F, size, size, 0, GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        // Depth renderbuffer
        glGenRenderbuffers(1, &m_depthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, m_depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthRbo);

        // attach face later in beginFace
        bool ok = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return ok;
    }

    void PointShadowMap::beginFace(int faceIndex)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex, m_cubemapTex, 0);
        GLenum drawBuf = GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1, &drawBuf);
        glViewport(0, 0, m_size, m_size);
        glClearColor(1e9f, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void PointShadowMap::end(int restoreViewportWidth, int restoreViewportHeight)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, restoreViewportWidth, restoreViewportHeight);
    }

    void PointShadowMap::bindCubemap(int slot) const
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemapTex);
    }

    void PointShadowMap::destroy()
    {
        if (m_depthRbo)
        {
            glDeleteRenderbuffers(1, &m_depthRbo);
            m_depthRbo = 0;
        }
        if (m_cubemapTex)
        {
            glDeleteTextures(1, &m_cubemapTex);
            m_cubemapTex = 0;
        }
        if (m_fbo)
        {
            glDeleteFramebuffers(1, &m_fbo);
            m_fbo = 0;
        }
    }
}


