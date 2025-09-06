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
            uniform int u_BloomEnabled; uniform sampler2D u_BloomSrc; uniform float u_BloomIntensity;
            uniform int u_TAAEnabled; uniform sampler2D u_History; uniform float u_TAAAlpha;
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
              if (u_TAAEnabled!=0){ vec3 hist = texture(u_History, vUV).rgb; hdr = mix(hdr, hist, u_TAAAlpha); }
              vec3 mapped = tonemapACES(hdr * u_Exposure);
              mapped = pow(mapped, vec3(1.0/u_Gamma));
              if (u_BloomEnabled!=0){ vec3 bloom = texture(u_BloomSrc, vUV).rgb; mapped += bloom * u_BloomIntensity; }
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
        // history
        glGenTextures(1, &m_colorTexHistory);
        glBindTexture(GL_TEXTURE_2D, m_colorTexHistory);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // ping/pong for bloom
        glGenTextures(1, &m_pingTex);
        glBindTexture(GL_TEXTURE_2D, m_pingTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenTextures(1, &m_pongTex);
        glBindTexture(GL_TEXTURE_2D, m_pongTex);
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
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTex, 0);
        glViewport(0, 0, width, height);
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void PostProcess::bind(int width, int height)
    {
        ensureSize(width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTex, 0);
        glViewport(0, 0, width, height);
    }

    void PostProcess::drawToScreen(int screenWidth, int screenHeight, float exposure, float gamma, bool fxaaEnabled,
                          bool bloomEnabled, float bloomThreshold, float bloomIntensity, int bloomIterations,
                          bool ssaoEnabled, float ssaoRadius, float ssaoBias, float ssaoPower,
                          bool taaEnabled, float taaAlpha)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screenWidth, screenHeight);
        glDisable(GL_DEPTH_TEST);
        // Simple bloom pre-pass: threshold into ping, blur ping->pong iterations
        if (bloomEnabled)
        {
            // threshold into ping
            glBindFramebuffer(GL_FRAMEBUFFER, m_fbo); // reuse fbo color attachment changes via drawbuffers?
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_pingTex, 0);
            glViewport(0, 0, m_width, m_height);
            if (!m_bloomShader)
            {
                const char* bvs = "#version 330 core\nlayout(location=0) in vec2 aPos; layout(location=1) in vec2 aUV; out vec2 vUV; void main(){ vUV=aUV; gl_Position=vec4(aPos,0,1);}";
                const char* bfs = "#version 330 core\nin vec2 vUV; out vec4 FragColor; uniform sampler2D u_Src; uniform float u_Threshold; void main(){ vec3 c=texture(u_Src,vUV).rgb; float l=max(max(c.r,c.g),c.b); FragColor=vec4(l>u_Threshold?c:vec3(0),1);}";
                m_bloomShader = std::make_unique<Shader>(); m_bloomShader->compileFromSource(bvs, bfs);
            }
            m_bloomShader->bind();
            m_bloomShader->setInt("u_Src", 0); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_colorTex);
            m_bloomShader->setFloat("u_Threshold", bloomThreshold);
            glBindVertexArray(m_vao); glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); glBindVertexArray(0);
            m_bloomShader->unbind();
            // blur ping->pong iterations
            if (!m_blurShader)
            {
                const char* blvs = "#version 330 core\nlayout(location=0) in vec2 aPos; layout(location=1) in vec2 aUV; out vec2 vUV; void main(){ vUV=aUV; gl_Position=vec4(aPos,0,1);}";
                const char* blfs = "#version 330 core\nin vec2 vUV; out vec4 FragColor; uniform sampler2D u_Src; uniform vec3 u_Dir; void main(){ vec2 texel=1.0/vec2(textureSize(u_Src,0)); vec3 s=vec3(0); float w[5]=float[](0.204164,0.304005,0.193783,0.072552,0.016996); for(int i=-4;i<=4;i++){ s+=texture(u_Src, vUV + u_Dir.xy*texel*i).rgb * w[abs(i)]; } FragColor=vec4(s,1);}";
                m_blurShader = std::make_unique<Shader>(); m_blurShader->compileFromSource(blvs, blfs);
            }
            for (int i=0;i<bloomIterations;i++)
            {
                // horizontal
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_pongTex, 0);
                m_blurShader->bind(); m_blurShader->setInt("u_Src",0); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_pingTex); m_blurShader->setVec3("u_Dir",1,0,0);
                glBindVertexArray(m_vao); glDrawArrays(GL_TRIANGLE_STRIP,0,4); glBindVertexArray(0); m_blurShader->unbind();
                // vertical back to ping
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_pingTex, 0);
                m_blurShader->bind(); m_blurShader->setInt("u_Src",0); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_pongTex); m_blurShader->setVec3("u_Dir",0,1,0);
                glBindVertexArray(m_vao); glDrawArrays(GL_TRIANGLE_STRIP,0,4); glBindVertexArray(0); m_blurShader->unbind();
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screenWidth, screenHeight);
        glDisable(GL_DEPTH_TEST);
        m_shader->bind();
        m_shader->setInt("u_Src", 0);
        m_shader->setFloat("u_Exposure", exposure);
        m_shader->setFloat("u_Gamma", gamma);
        m_shader->setInt("u_FXAA", fxaaEnabled ? 1 : 0);
        m_shader->setInt("u_BloomEnabled", bloomEnabled?1:0);
        m_shader->setInt("u_BloomSrc", 1);
        m_shader->setFloat("u_BloomIntensity", bloomIntensity);
        m_shader->setInt("u_TAAEnabled", taaEnabled?1:0);
        m_shader->setInt("u_History", 2);
        m_shader->setFloat("u_TAAAlpha", taaAlpha);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_colorTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bloomEnabled ? m_pingTex : m_colorTex);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, m_colorTexHistory);
        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        m_shader->unbind();
        glEnable(GL_DEPTH_TEST);
        // copy current color into history for next frame TAA
        if (taaEnabled)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTexHistory, 0);
            glViewport(0, 0, m_width, m_height);
            if (!m_copyShader)
            {
                const char* cvs = "#version 330 core\nlayout(location=0) in vec2 aPos; layout(location=1) in vec2 aUV; out vec2 vUV; void main(){ vUV=aUV; gl_Position=vec4(aPos,0,1);}";
                const char* cfs = "#version 330 core\nin vec2 vUV; out vec4 FragColor; uniform sampler2D u_Src; void main(){ FragColor = texture(u_Src, vUV); }";
                m_copyShader = std::make_unique<Shader>(); m_copyShader->compileFromSource(cvs, cfs);
            }
            m_copyShader->bind(); m_copyShader->setInt("u_Src", 0); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_colorTex);
            glBindVertexArray(m_vao); glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); glBindVertexArray(0);
            m_copyShader->unbind();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }

    void PostProcess::destroy()
    {
        if (m_ssaoTex) { glDeleteTextures(1, &m_ssaoTex); m_ssaoTex = 0; }
        if (m_pingTex) { glDeleteTextures(1, &m_pingTex); m_pingTex = 0; }
        if (m_pongTex) { glDeleteTextures(1, &m_pongTex); m_pongTex = 0; }
        if (m_colorTexHistory) { glDeleteTextures(1, &m_colorTexHistory); m_colorTexHistory = 0; }
        if (m_depthRbo) { glDeleteRenderbuffers(1, &m_depthRbo); m_depthRbo = 0; }
        if (m_colorTex) { glDeleteTextures(1, &m_colorTex); m_colorTex = 0; }
        if (m_fbo) { glDeleteFramebuffers(1, &m_fbo); m_fbo = 0; }
        if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
        if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
        m_shader.reset();
        m_bloomShader.reset(); m_blurShader.reset(); m_ssaoShader.reset();
        m_width = m_height = 0;
    }
}


