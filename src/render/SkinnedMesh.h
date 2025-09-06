#pragma once

#include <vector>

namespace engine
{
    class SkinnedMesh
    {
    public:
        SkinnedMesh() = default;
        ~SkinnedMesh();

        SkinnedMesh(const SkinnedMesh&) = delete;
        SkinnedMesh& operator=(const SkinnedMesh&) = delete;
        SkinnedMesh(SkinnedMesh&&) noexcept;
        SkinnedMesh& operator=(SkinnedMesh&&) noexcept;

        // vertices: pos(3)+normal(3)+uv(2) as floats (8 floats per vertex)
        // boneIds: 4 uint per vertex
        // weights: 4 floats per vertex
        bool create(const std::vector<float>& verticesPNU,
                    const std::vector<unsigned int>& boneIds,
                    const std::vector<float>& weights,
                    const std::vector<unsigned int>& indices);
        void draw() const;
        void destroy();

        unsigned int indexCount() const { return m_indexCount; }
        unsigned int vertexCount() const { return m_vertexCount; }

    private:
        unsigned int m_vao = 0;
        unsigned int m_vboPNU = 0; // positions+normals+uvs
        unsigned int m_vboIds = 0;  // bone ids (ivec4)
        unsigned int m_vboW = 0;    // weights (vec4)
        unsigned int m_ebo = 0;
        unsigned int m_indexCount = 0;
        unsigned int m_vertexCount = 0;
    };
}


