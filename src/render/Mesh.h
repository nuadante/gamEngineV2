#pragma once

#include <vector>
#include <glm/glm.hpp>

namespace engine
{
    class Mesh
    {
    public:
        Mesh() = default;
        ~Mesh();

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;
        Mesh(Mesh&& other) noexcept;
        Mesh& operator=(Mesh&& other) noexcept;

        // vertices: position (3) + normal (3) + uv (2) = 8 floats per vertex
        bool create(const std::vector<float>& vertices, const std::vector<unsigned int>& indices);
        void draw() const;
        void drawInstanced(int count) const;
        bool setInstanceTransforms(const std::vector<glm::mat4>& instanceMatrices);
        void destroy();

        static Mesh createCube();
        static Mesh createPlane();

    private:
        unsigned int m_vao = 0;
        unsigned int m_vbo = 0;
        unsigned int m_ebo = 0;
        unsigned int m_indexCount = 0;
        unsigned int m_instanceVBO = 0;
    };
}


