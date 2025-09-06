#pragma once

#include <memory>
#include <vector>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace engine
{
    class Shader;
    class Texture2D;

    class Terrain
    {
    public:
        Terrain();
        ~Terrain();

        bool initialize(int gridResolution);
        void destroy();

        void setHeightmap(Texture2D* tex) { m_heightmap = tex; }
        void setSplatControl(Texture2D* tex) { m_splatControl = tex; }
        void setSplatTexture(int idx, Texture2D* tex);
        void setNormalMap(Texture2D* tex) { m_normalMap = tex; }

        void setParams(float heightScale, float splatTiling, float cellWorldSize);
        void setLOD(int level); // 0=high,1=med,2=low

        void draw(const glm::mat4& proj, const glm::mat4& view,
                  const glm::vec3& cameraPos,
                  const glm::vec3& lightPos,
                  const glm::vec3& lightColor);

    private:
        bool createMesh(int grid);
        bool createShader();

    private:
        std::unique_ptr<Shader> m_shader;
        unsigned int m_vao = 0;
        unsigned int m_vbo = 0; // aXZ, aUV -> 4 floats
        unsigned int m_eboHigh = 0, m_eboMed = 0, m_eboLow = 0;
        unsigned int m_idxHigh = 0, m_idxMed = 0, m_idxLow = 0;
        int m_lod = 0;

        Texture2D* m_heightmap = nullptr;
        Texture2D* m_splatControl = nullptr;
        Texture2D* m_splat[4] = { nullptr, nullptr, nullptr, nullptr };
        Texture2D* m_normalMap = nullptr;

        float m_heightScale = 10.0f;
        float m_splatTiling = 16.0f;
        float m_cellWorld = 1.0f;
        int m_gridRes = 0;
    };
}


