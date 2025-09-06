#include "render/IBL.h"
#include "render/Shader.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>
#include <vector>
#include <iostream>

namespace engine
{
    static const float CUBE_VERTS[] = {
        // positions
        -1.0f,-1.0f,-1.0f,  -1.0f,-1.0f, 1.0f,  -1.0f, 1.0f, 1.0f,   1.0f, 1.0f,-1.0f,
         1.0f,-1.0f, 1.0f, -1.0f,-1.0f,-1.0f, -1.0f, 1.0f,-1.0f,   1.0f,-1.0f,-1.0f,
        -1.0f,-1.0f,-1.0f, -1.0f, 1.0f, 1.0f,   1.0f,-1.0f, 1.0f,   1.0f, 1.0f, 1.0f,
         1.0f,-1.0f, 1.0f, -1.0f,-1.0f, 1.0f, -1.0f,-1.0f,-1.0f,   1.0f,-1.0f,-1.0f,
        -1.0f,-1.0f,-1.0f, -1.0f, 1.0f,-1.0f,   1.0f, 1.0f,-1.0f,   1.0f, 1.0f, 1.0f,
         1.0f, 1.0f,-1.0f, -1.0f, 1.0f,-1.0f, -1.0f,-1.0f,-1.0f,  -1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f,-1.0f,  -1.0f, 1.0f, 1.0f,  1.0f, 1.0f, 1.0f,   1.0f, 1.0f, 1.0f,
         1.0f, 1.0f,-1.0f, -1.0f,-1.0f,-1.0f, -1.0f, 1.0f,-1.0f,  -1.0f, 1.0f, 1.0f,
        -1.0f,-1.0f, 1.0f,  1.0f, 1.0f, 1.0f,  -1.0f,-1.0f, 1.0f,  -1.0f, 1.0f, 1.0f
    };

    static const float QUAD_VERTS[] = {
        // pos   // uv
        -1.f, -1.f,  0.f, 0.f,
         1.f, -1.f,  1.f, 0.f,
        -1.f,  1.f,  0.f, 1.f,
         1.f,  1.f,  1.f, 1.f,
    };

    IBL::IBL() {}
    IBL::~IBL() { destroy(); }

    void IBL::renderCube()
    {
        if (m_cubeVAO == 0)
        {
            glGenVertexArrays(1, &m_cubeVAO);
            glGenBuffers(1, &m_cubeVBO);
            glBindVertexArray(m_cubeVAO);
            glBindBuffer(GL_ARRAY_BUFFER, m_cubeVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(CUBE_VERTS), CUBE_VERTS, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        }
        glBindVertexArray(m_cubeVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 36);
        glBindVertexArray(0);
    }

    void IBL::renderQuad()
    {
        if (m_quadVAO == 0)
        {
            glGenVertexArrays(1, &m_quadVAO);
            glGenBuffers(1, &m_quadVBO);
            glBindVertexArray(m_quadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTS), QUAD_VERTS, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2*sizeof(float)));
        }
        glBindVertexArray(m_quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
    }

    bool IBL::createCapture()
    {
        if (m_captureFBO) return true;
        glGenFramebuffers(1, &m_captureFBO);
        glGenRenderbuffers(1, &m_captureRBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
        glBindRenderbuffer(GL_RENDERBUFFER, m_captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_captureRBO);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }

    bool IBL::createFromHDR(const std::string& hdrPath)
    {
        destroy();
        createCapture();

        stbi_set_flip_vertically_on_load(true);
        int w, h, n;
        float* data = stbi_loadf(hdrPath.c_str(), &w, &h, &n, 0);
        if (!data) { std::cerr << "[IBL] HDR load failed: " << hdrPath << std::endl; return false; }
        unsigned int hdrTex; glGenTextures(1, &hdrTex);
        glBindTexture(GL_TEXTURE_2D, hdrTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        stbi_image_free(data);

        glGenTextures(1, &m_envCubemap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
        for (int i=0;i<6;++i)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i, 0, GL_RGB16F, 512,512,0,GL_RGB,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        // shaders
        const char* eqVS = R"GLSL(
            #version 330 core
            layout(location=0) in vec3 aPos; uniform mat4 u_VP; out vec3 vDir; void main(){ vDir = aPos; gl_Position = u_VP * vec4(aPos,1.0);} 
        )GLSL";
        const char* eqFS = R"GLSL(
            #version 330 core
            in vec3 vDir; out vec4 FragColor; uniform sampler2D u_HDR; const float PI=3.14159265; 
            void main(){ vec3 d = normalize(vDir); float phi = atan(d.z, d.x); float theta = acos(d.y); vec2 uv = vec2(phi/(2.0*PI)+0.5, theta/PI); vec3 col = texture(u_HDR, uv).rgb; FragColor = vec4(col,1.0);} 
        )GLSL";
        m_equirectShader = std::make_unique<Shader>();
        if (!m_equirectShader->compileFromSource(eqVS, eqFS)) { glDeleteTextures(1,&hdrTex); return false; }

        glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        glm::mat4 views[6] = {
            glm::lookAt(glm::vec3(0,0,0), glm::vec3(1,0,0), glm::vec3(0,-1,0)),
            glm::lookAt(glm::vec3(0,0,0), glm::vec3(-1,0,0), glm::vec3(0,-1,0)),
            glm::lookAt(glm::vec3(0,0,0), glm::vec3(0,1,0), glm::vec3(0,0,1)),
            glm::lookAt(glm::vec3(0,0,0), glm::vec3(0,-1,0), glm::vec3(0,0,-1)),
            glm::lookAt(glm::vec3(0,0,0), glm::vec3(0,0,1), glm::vec3(0,-1,0)),
            glm::lookAt(glm::vec3(0,0,0), glm::vec3(0,0,-1), glm::vec3(0,-1,0))
        };

        glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
        glBindRenderbuffer(GL_RENDERBUFFER, m_captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512,512);
        m_equirectShader->bind();
        m_equirectShader->setInt("u_HDR",0);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, hdrTex);
        for (int i=0;i<6;++i){
            glm::mat4 vp = proj * views[i];
            m_equirectShader->setMat4("u_VP", &vp[0][0]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X+i, m_envCubemap, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderCube();
        }
        m_equirectShader->unbind();

        // irradiance
        glGenTextures(1, &m_irradianceMap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_irradianceMap);
        for (int i=0;i<6;++i)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i,0,GL_RGB16F,32,32,0,GL_RGB,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glBindRenderbuffer(GL_RENDERBUFFER, m_captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32,32);

        const char* irrVS = eqVS;
        const char* irrFS = R"GLSL(
            #version 330 core
            in vec3 vDir; out vec4 FragColor; uniform samplerCube u_Env; const float PI=3.14159265;
            void main(){ vec3 N = normalize(vDir); vec3 up = vec3(0.0,1.0,0.0); vec3 right = normalize(cross(up, N)); up = cross(N, right);
              float sampleDelta = 0.025; vec3 irradiance = vec3(0.0);
              for(float phi=0.0; phi<2.0*PI; phi += sampleDelta){ for(float theta=0.0; theta<0.5*PI; theta += sampleDelta){ vec3 tangentSample = vec3(sin(theta)*cos(phi), sin(theta)*sin(phi), cos(theta)); vec3 sampleVec = tangentSample.x*right + tangentSample.y*up + tangentSample.z*N; irradiance += texture(u_Env, sampleVec).rgb * cos(theta) * sin(theta); }}
              irradiance = PI * irradiance * (sampleDelta*sampleDelta);
              FragColor = vec4(irradiance, 1.0);
            }
        )GLSL";
        m_irradianceShader = std::make_unique<Shader>(); m_irradianceShader->compileFromSource(irrVS, irrFS);
        m_irradianceShader->bind(); m_irradianceShader->setInt("u_Env",0);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
        for (int i=0;i<6;++i){ glm::mat4 vp = proj * views[i]; m_irradianceShader->setMat4("u_VP", &vp[0][0]); glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X+i, m_irradianceMap, 0); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT); renderCube(); }
        m_irradianceShader->unbind();

        // prefilter
        glGenTextures(1, &m_prefilterMap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_prefilterMap);
        const int preSize = 128;
        for (int i=0;i<6;++i)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i,0,GL_RGB16F,preSize,preSize,0,GL_RGB,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

        const char* pfVS = eqVS;
        const char* pfFS = R"GLSL(
            #version 330 core
            in vec3 vDir; out vec4 FragColor; uniform samplerCube u_Env; uniform float u_Roughness; const float PI=3.14159265;
            float VanDerCorpus(uint n, uint base){ float inv = 1.0/float(base); float denom = 1.0; float res = 0.0; for(uint i=0u;i<32u;i++){ if((n & 1u) == 1u) res += inv/denom; denom*=base; n>>=1u;} return res; }
            vec2 Hammersley(uint i, uint N){ return vec2(float(i)/float(N), VanDerCorpus(i,2u)); }
            vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float rough){ float a=rough*rough; float phi=2.0*PI*Xi.x; float cosTheta = sqrt((1.0 - Xi.y)/(1.0 + (a*a - 1.0)*Xi.y)); float sinTheta = sqrt(1.0 - cosTheta*cosTheta); vec3 H = vec3(cos(phi)*sinTheta, sin(phi)*sinTheta, cosTheta); vec3 up = abs(N.z)<0.999? vec3(0,0,1):vec3(1,0,0); vec3 T = normalize(cross(up,N)); vec3 B = cross(N,T); return normalize(T*H.x + B*H.y + N*H.z); }
            void main(){ vec3 N = normalize(vDir); vec3 R = N; vec3 V = R; const uint SAMPLE_COUNT = 1024u; vec3 prefiltered = vec3(0.0); float total=0.0; for(uint i=0u;i<SAMPLE_COUNT;i++){ vec2 Xi = Hammersley(i, SAMPLE_COUNT); vec3 H = ImportanceSampleGGX(Xi, N, u_Roughness); vec3 L = normalize(2.0*dot(V,H)*H - V); float NdotL = max(dot(N,L),0.0); if(NdotL>0.0){ prefiltered += texture(u_Env, L).rgb * NdotL; total += NdotL; }} prefiltered = prefiltered / max(total, 0.001); FragColor = vec4(prefiltered,1.0); }
        )GLSL";
        m_prefilterShader = std::make_unique<Shader>(); m_prefilterShader->compileFromSource(pfVS, pfFS);
        m_prefilterShader->bind(); m_prefilterShader->setInt("u_Env",0); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
        const int maxMip = 5;
        for (int mip=0; mip<maxMip; ++mip){ int sz = preSize >> mip; glBindRenderbuffer(GL_RENDERBUFFER, m_captureRBO); glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, sz, sz); float rough = (float)mip/(float)(maxMip-1); m_prefilterShader->setFloat("u_Roughness", rough); for (int i=0;i<6;++i){ glm::mat4 vp = proj * views[i]; m_prefilterShader->setMat4("u_VP", &vp[0][0]); glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X+i, m_prefilterMap, mip); glViewport(0,0,sz,sz); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT); renderCube(); }}
        m_prefilterShader->unbind();

        // BRDF LUT 512x512
        glGenTextures(1, &m_brdfLUT);
        glBindTexture(GL_TEXTURE_2D, m_brdfLUT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512,512,0,GL_RG,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        const char* brdfVS = R"GLSL(
            #version 330 core
            layout(location=0) in vec2 aPos; layout(location=1) in vec2 aUV; out vec2 vUV; void main(){ vUV=aUV; gl_Position=vec4(aPos,0,1);} 
        )GLSL";
        const char* brdfFS = R"GLSL(
            #version 330 core
            in vec2 vUV; out vec2 FragColor; const float PI=3.14159265;
            float GeometrySchlickGGX(float NdotV, float k){ return NdotV/(NdotV*(1.0-k)+k); }
            float GeometrySmith(float NdotV, float NdotL, float k){ return GeometrySchlickGGX(NdotV,k)*GeometrySchlickGGX(NdotL,k); }
            vec2 IntegrateBRDF(float NdotV, float rough){ vec3 V=vec3(sqrt(1.0 - NdotV*NdotV), 0.0, NdotV); float A=0.0; float B=0.0; const int SAMPLE_COUNT=1024; for(int i=0;i<SAMPLE_COUNT;i++){ float Xi1=float(i)/float(SAMPLE_COUNT); float Xi2=fract(sin(float(i)*12.9898)*43758.5453); float a=rough*rough; float phi=2.0*PI*Xi1; float cosTheta=sqrt((1.0 - Xi2)/(1.0 + (a*a -1.0)*Xi2)); float sinTheta=sqrt(1.0 - cosTheta*cosTheta); vec3 H=vec3(cos(phi)*sinTheta, sin(phi)*sinTheta, cosTheta); vec3 L=normalize(2.0*dot(V,H)*H - V); float NdotL=max(L.z,0.0); float NdotH=max(H.z,0.0); float VdotH=max(dot(V,H),0.0); if(NdotL>0.0){ float G=GeometrySmith(NdotV, NdotL, (rough+1.0)*(rough+1.0)/8.0); float G_Vis = (G*VdotH)/(NdotH*max(NdotV,0.0)); float Fc = pow(1.0 - VdotH, 5.0); A += (1.0 - Fc)*G_Vis; B += Fc*G_Vis; }} A/=float(SAMPLE_COUNT); B/=float(SAMPLE_COUNT); return vec2(A,B); }
            void main(){ float NdotV = vUV.x; float rough = vUV.y; FragColor = IntegrateBRDF(NdotV, rough); }
        )GLSL";
        m_brdfShader = std::make_unique<Shader>(); m_brdfShader->compileFromSource(brdfVS, brdfFS);
        glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_brdfLUT, 0);
        glViewport(0,0,512,512); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        m_brdfShader->bind(); renderQuad(); m_brdfShader->unbind();

        // cleanup
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteTextures(1, &hdrTex);
        return true;
    }

    void IBL::destroy()
    {
        if (m_brdfLUT) { glDeleteTextures(1, &m_brdfLUT); m_brdfLUT = 0; }
        if (m_prefilterMap) { glDeleteTextures(1, &m_prefilterMap); m_prefilterMap = 0; }
        if (m_irradianceMap) { glDeleteTextures(1, &m_irradianceMap); m_irradianceMap = 0; }
        if (m_envCubemap) { glDeleteTextures(1, &m_envCubemap); m_envCubemap = 0; }
        if (m_cubeVBO) { glDeleteBuffers(1, &m_cubeVBO); m_cubeVBO = 0; }
        if (m_cubeVAO) { glDeleteVertexArrays(1, &m_cubeVAO); m_cubeVAO = 0; }
        if (m_quadVBO) { glDeleteBuffers(1, &m_quadVBO); m_quadVBO = 0; }
        if (m_quadVAO) { glDeleteVertexArrays(1, &m_quadVAO); m_quadVAO = 0; }
        if (m_captureRBO) { glDeleteRenderbuffers(1, &m_captureRBO); m_captureRBO = 0; }
        if (m_captureFBO) { glDeleteFramebuffers(1, &m_captureFBO); m_captureFBO = 0; }
        m_equirectShader.reset(); m_irradianceShader.reset(); m_prefilterShader.reset(); m_brdfShader.reset();
    }
}


