#include "render/PostProcess.h"
#include "render/Shader.h"

#include <glad/glad.h>

namespace engine
{
    static const float kQuad[] = {
        // pos   // uv
        -1.f, -1.f,  0.f, 0.f,
         1.f, -1.f,  1.f, 0.f,
        -1.f,  1.f,  0.f, 1.f,
         1.f,  1.f,  1.f, 1.f,
    };

    PostProcess::PostProcess() = default;
    PostProcess::~PostProcess() { destroy(); }

    bool PostProcess::createQuad()
    {
        if (m_vao) return true;
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
         glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glBindVertexArray(0);
        return true;
    }

    bool PostProcess::createShader()
    {
        if (m_shader) return true;
        const char* vs = R"GLSL(
            #version 330 core
            layout (location = 0) in vec2 aPos;
            layout (location = 1) in vec2 aUV;
            out vec2 vUV;
            void main(){ vUV = aUV; gl_Position = vec4(aPos, 0.0, 1.0); }
        )GLSL";
        const char* fs = R"GLSL(
            #version 330 core
            in vec2 vUV; out vec4 FragColor;
            uniform sampler2D u_Src;
            uniform float u_Exposure;
            uniform float u_Gamma;
            uniform int u_FXAA;
            vec3 tonemapACES(vec3 x){
              float a=2.51,b=0.03,c=2.43,d=0.59,e=0.14; return clamp((x*(a*x+b))/(x*(c*x+d)+e),0.0,1.0);
            }
            vec3 fxaa(sampler2D tex, vec2 uv, vec2 texel){
              vec3 rgbNW = texture(tex, uv + texel * vec2(-1.0,-1.0)).rgb;
              vec3 rgbNE = texture(tex, uv + texel * vec2( 1.0,-1.0)).rgb;
              vec3 rgbSW = texture(tex, uv + texel * vec2(-1.0, 1.0)).rgb;
              vec3 rgbSE = texture(tex, uv + texel * vec2( 1.0, 1.0)).rgb;
              vec3 rgbM  = texture(tex, uv).rgb;
              vec3 luma = vec3(0.299,0.587,0.114);
              float lumaM = dot(rgbM, luma);
              float lumaMin = min(lumaM, min(min(dot(rgbNW,luma), dot(rgbNE,luma)), min(dot(rgbSW,luma), dot(rgbSE,luma))));
              float lumaMax = max(lumaM, max(max(dot(rgbNW,luma), dot(rgbNE,luma)), max(dot(rgbSW,luma), dot(rgbSE,luma))));
              if (lumaMax - lumaMin < 0.031) return rgbM;
              vec3 rgbBlur = (rgbNW + rgbNE + rgbSW + rgbSE + rgbM) / 5.0;
              return mix(rgbM, rgbBlur, 0.5);
            }
            void main(){
              vec2 texel = 1.0/vec2(textureSize(u_Src,0));
              vec3 hdr = (u_FXAA!=0) ? fxaa(u_Src, vUV, texel) : texture(u_Src, vUV).rgb;
              vec3 mapped = tonemapACES(hdr * u_Exposure);
              mapped = pow(mapped, vec3(1.0/u_Gamma));
              FragColor = vec4(mapped, 1.0);
            }
        )GLSL";
        m_shader = std::make_unique<Shader>();
        return m_shader->compileFromSource(vs, fs);
    }

    bool PostProcess::create(int width, int height)
    {
        m_width = width; m_height = height;
        if (!createQuad()) return false;
        if (!createShader()) return false;
        if (m_fbo) destroy();
        glGenFramebuffers(1, &m_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glGenTextures(1, &m_colorTex);
        glBindTexture(GL_TEXTURE_2D, m_colorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenRenderbuffers(1, &m_depthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, m_depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthRbo);
        GLenum drawBuf = GL_COLOR_ATTACHMENT0; glDrawBuffers(1, &drawBuf);
        bool ok = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return ok;
    }

    bool PostProcess::ensureSize(int width, int height)
    {
        if (width == m_width && height == m_height && m_fbo) return true;
        destroy();
        return create(width, height);
    }

    void PostProcess::begin(int width, int height, float r, float g, float b, float a)
    {
        ensureSize(width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, width, height);
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void PostProcess::bind(int width, int height)
    {
        ensureSize(width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, width, height);
    }

    void PostProcess::drawToScreen(int screenWidth, int screenHeight, float exposure, float gamma, bool fxaaEnabled)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screenWidth, screenHeight);
        glDisable(GL_DEPTH_TEST);
        m_shader->bind();
        m_shader->setInt("u_Src", 0);
        m_shader->setFloat("u_Exposure", exposure);
        m_shader->setFloat("u_Gamma", gamma);
        m_shader->setInt("u_FXAA", fxaaEnabled ? 1 : 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_colorTex);
        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        m_shader->unbind();
        glEnable(GL_DEPTH_TEST);
    }

    void PostProcess::destroy()
    {
        if (m_depthRbo) { glDeleteRenderbuffers(1, &m_depthRbo); m_depthRbo = 0; }
        if (m_colorTex) { glDeleteTextures(1, &m_colorTex); m_colorTex = 0; }
        if (m_fbo) { glDeleteFramebuffers(1, &m_fbo); m_fbo = 0; }
        if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
        if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
        m_shader.reset();
        m_width = m_height = 0;
    }
}


