#include "render/Terrain.h"
#include "render/Shader.h"
#include "render/Texture2D.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

namespace engine
{
    Terrain::Terrain() = default;
    Terrain::~Terrain() { destroy(); }

    void Terrain::setSplatTexture(int idx, Texture2D* tex)
    {
        if (idx >= 0 && idx < 4) m_splat[idx] = tex;
    }

    void Terrain::setParams(float heightScale, float splatTiling, float cellWorldSize)
    {
        m_heightScale = heightScale; m_splatTiling = splatTiling; m_cellWorld = cellWorldSize;
    }

    void Terrain::setLOD(int level)
    {
        m_lod = level < 0 ? 0 : (level > 2 ? 2 : level);
    }

    bool Terrain::createShader()
    {
        if (m_shader) return true;
        const char* vs = R"GLSL(
            #version 330 core
            layout (location = 0) in vec2 aXZ;
            layout (location = 1) in vec2 aUV;
            uniform mat4 u_Proj;
            uniform mat4 u_View;
            uniform float u_CellWorld;
            uniform float u_HeightScale;
            uniform sampler2D u_Heightmap;
            out vec2 vUV;
            out vec3 vWorldPos;
            out vec3 vNormal;
            vec3 calcNormal(vec2 uv){
                float hL = texture(u_Heightmap, uv + vec2(-1,0)/textureSize(u_Heightmap,0)).r;
                float hR = texture(u_Heightmap, uv + vec2( 1,0)/textureSize(u_Heightmap,0)).r;
                float hD = texture(u_Heightmap, uv + vec2(0,-1)/textureSize(u_Heightmap,0)).r;
                float hU = texture(u_Heightmap, uv + vec2(0, 1)/textureSize(u_Heightmap,0)).r;
                vec3 n = normalize(vec3(hL - hR, 2.0, hD - hU));
                return n;
            }
            void main(){
                vUV = aUV;
                float h = texture(u_Heightmap, aUV).r * u_HeightScale;
                vec3 wp = vec3(aXZ.x * u_CellWorld, h, aXZ.y * u_CellWorld);
                vWorldPos = wp;
                vNormal = calcNormal(aUV);
                gl_Position = u_Proj * u_View * vec4(wp,1.0);
            }
        )GLSL";
        const char* fs = R"GLSL(
            #version 330 core
            in vec2 vUV; in vec3 vWorldPos; in vec3 vNormal;
            out vec4 FragColor;
            uniform vec3 u_CameraPos; uniform vec3 u_LightPos; uniform vec3 u_LightColor;
            uniform sampler2D u_SplatCtrl; // RGBA: layer weights
            uniform sampler2D u_Splat0; uniform sampler2D u_Splat1; uniform sampler2D u_Splat2; uniform sampler2D u_Splat3;
            uniform float u_SplatTiling;
            uniform sampler2D u_NormalMap; // optional, tangent-free approximation
            void main(){
                vec4 w = texture(u_SplatCtrl, vUV);
                vec2 tuv = vUV * u_SplatTiling;
                vec3 albedo = vec3(0.0);
                albedo += w.r * texture(u_Splat0, tuv).rgb;
                albedo += w.g * texture(u_Splat1, tuv).rgb;
                albedo += w.b * texture(u_Splat2, tuv).rgb;
                albedo += w.a * texture(u_Splat3, tuv).rgb;
                vec3 N = normalize(vNormal);
                vec3 L = normalize(u_LightPos - vWorldPos);
                vec3 V = normalize(u_CameraPos - vWorldPos);
                vec3 H = normalize(L + V);
                float diff = max(dot(N,L),0.0);
                float spec = pow(max(dot(N,H),0.0), 32.0);
                vec3 color = albedo * (0.1 + diff) + u_LightColor * spec;
                FragColor = vec4(color,1.0);
            }
        )GLSL";
        m_shader = std::make_unique<Shader>();
        return m_shader->compileFromSource(vs, fs);
    }

    bool Terrain::createMesh(int grid)
    {
        m_gridRes = grid;
        std::vector<float> verts; verts.reserve(grid * grid * 4);
        std::vector<unsigned int> idxHigh; idxHigh.reserve((grid-1)*(grid-1)*6);
        for (int z = 0; z < grid; ++z)
        {
            for (int x = 0; x < grid; ++x)
            {
                float fx = (float)x; float fz = (float)z;
                float u = fx / (grid - 1); float v = fz / (grid - 1);
                verts.push_back(fx); verts.push_back(fz); // aXZ
                verts.push_back(u); verts.push_back(v);   // aUV
            }
        }
        auto vid = [&](int x, int z){ return z * grid + x; };
        for (int z = 0; z < grid - 1; ++z)
        {
            for (int x = 0; x < grid - 1; ++x)
            {
                unsigned a = vid(x, z);
                unsigned b = vid(x+1, z);
                unsigned c = vid(x, z+1);
                unsigned d = vid(x+1, z+1);
                idxHigh.push_back(a); idxHigh.push_back(b); idxHigh.push_back(c);
                idxHigh.push_back(b); idxHigh.push_back(d); idxHigh.push_back(c);
            }
        }
        // Create reduced LODs by sampling every 2 and 4
        auto buildLOD = [&](int step, unsigned int& ebo, unsigned int& count){
            std::vector<unsigned> idx;
            for (int z = 0; z < grid - 1; z += step)
            for (int x = 0; x < grid - 1; x += step)
            {
                unsigned a = vid(x, z);
                unsigned b = vid(x+step, z);
                unsigned c = vid(x, z+step);
                unsigned d = vid(x+step, z+step);
                idx.push_back(a); idx.push_back(b); idx.push_back(c);
                idx.push_back(b); idx.push_back(d); idx.push_back(c);
            }
            glGenBuffers(1, &ebo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
            count = (unsigned)idx.size();
        };

        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);
        glGenBuffers(1, &m_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));

        glGenBuffers(1, &m_eboHigh);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_eboHigh);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxHigh.size()*sizeof(unsigned), idxHigh.data(), GL_STATIC_DRAW);
        m_idxHigh = (unsigned)idxHigh.size();

        buildLOD(2, m_eboMed, m_idxMed);
        buildLOD(4, m_eboLow, m_idxLow);

        glBindVertexArray(0);
        return true;
    }

    bool Terrain::initialize(int gridResolution)
    {
        if (!createShader()) return false;
        if (!createMesh(gridResolution)) return false;
        return true;
    }

    void Terrain::draw(const glm::mat4& proj, const glm::mat4& view,
                       const glm::vec3& cameraPos,
                       const glm::vec3& lightPos,
                       const glm::vec3& lightColor)
    {
        if (!m_heightmap) return;
        m_shader->bind();
        m_shader->setMat4("u_Proj", &proj[0][0]);
        m_shader->setMat4("u_View", &view[0][0]);
        m_shader->setFloat("u_CellWorld", m_cellWorld);
        m_shader->setFloat("u_HeightScale", m_heightScale);
        m_shader->setVec3("u_CameraPos", cameraPos.x, cameraPos.y, cameraPos.z);
        m_shader->setVec3("u_LightPos", lightPos.x, lightPos.y, lightPos.z);
        m_shader->setVec3("u_LightColor", lightColor.x, lightColor.y, lightColor.z);

        m_shader->setInt("u_Heightmap", 0);
        m_shader->setInt("u_SplatCtrl", 1);
        m_shader->setInt("u_Splat0", 2);
        m_shader->setInt("u_Splat1", 3);
        m_shader->setInt("u_Splat2", 4);
        m_shader->setInt("u_Splat3", 5);
        m_shader->setInt("u_NormalMap", 6);
        m_shader->setFloat("u_SplatTiling", m_splatTiling);

        if (m_heightmap) m_heightmap->bind(0);
        if (m_splatControl) m_splatControl->bind(1);
        for (int i = 0; i < 4; ++i) if (m_splat[i]) m_splat[i]->bind(2+i);
        if (m_normalMap) m_normalMap->bind(6);

        glBindVertexArray(m_vao);
        if (m_lod == 0) { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_eboHigh); glDrawElements(GL_TRIANGLES, m_idxHigh, GL_UNSIGNED_INT, 0); }
        else if (m_lod == 1) { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_eboMed); glDrawElements(GL_TRIANGLES, m_idxMed, GL_UNSIGNED_INT, 0); }
        else { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_eboLow); glDrawElements(GL_TRIANGLES, m_idxLow, GL_UNSIGNED_INT, 0); }
        glBindVertexArray(0);
        m_shader->unbind();
    }

    void Terrain::destroy()
    {
        if (m_eboLow) { glDeleteBuffers(1, &m_eboLow); m_eboLow = 0; }
        if (m_eboMed) { glDeleteBuffers(1, &m_eboMed); m_eboMed = 0; }
        if (m_eboHigh) { glDeleteBuffers(1, &m_eboHigh); m_eboHigh = 0; }
        if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
        if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
        m_shader.reset();
    }
}


