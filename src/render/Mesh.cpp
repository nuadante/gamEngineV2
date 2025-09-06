#include "render/Mesh.h"

#include <glad/glad.h>

namespace engine
{
    Mesh::~Mesh() { destroy(); }

    Mesh::Mesh(Mesh&& other) noexcept
    {
        m_vao = other.m_vao; other.m_vao = 0;
        m_vbo = other.m_vbo; other.m_vbo = 0;
        m_ebo = other.m_ebo; other.m_ebo = 0;
        m_indexCount = other.m_indexCount; other.m_indexCount = 0;
    }

    Mesh& Mesh::operator=(Mesh&& other) noexcept
    {
        if (this == &other) return *this;
        destroy();
        m_vao = other.m_vao; other.m_vao = 0;
        m_vbo = other.m_vbo; other.m_vbo = 0;
        m_ebo = other.m_ebo; other.m_ebo = 0;
        m_indexCount = other.m_indexCount; other.m_indexCount = 0;
        return *this;
    }

    bool Mesh::create(const std::vector<float>& vertices, const std::vector<unsigned int>& indices)
    {
        m_indexCount = static_cast<unsigned int>(indices.size());

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glGenBuffers(1, &m_ebo);

        glBindVertexArray(m_vao);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        // layout: position (0), normal (1), uv (2)
        GLsizei stride = 8 * sizeof(float);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

        glBindVertexArray(0);
        return true;
    }

    void Mesh::draw() const
    {
        glBindVertexArray(m_vao);
        glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    void Mesh::drawInstanced(int count) const
    {
        glBindVertexArray(m_vao);
        glDrawElementsInstanced(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0, count);
        glBindVertexArray(0);
    }

    bool Mesh::setInstanceTransforms(const std::vector<glm::mat4>& instanceMatrices)
    {
        if (m_vao == 0) return false;
        if (m_instanceVBO == 0)
            glGenBuffers(1, &m_instanceVBO);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, instanceMatrices.size()*sizeof(glm::mat4), instanceMatrices.data(), GL_DYNAMIC_DRAW);
        // set attributes 3,4,5,6 as mat4 (vec4 per column)
        std::size_t vec4Size = sizeof(float)*4;
        for (int i=0;i<4;++i)
        {
            glEnableVertexAttribArray(3+i);
            glVertexAttribPointer(3+i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i*vec4Size));
            glVertexAttribDivisor(3+i, 1);
        }
        glBindVertexArray(0);
        return true;
    }

    void Mesh::destroy()
    {
        if (m_instanceVBO) { glDeleteBuffers(1, &m_instanceVBO); m_instanceVBO = 0; }
        if (m_ebo) { glDeleteBuffers(1, &m_ebo); m_ebo = 0; }
        if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
        if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
        m_indexCount = 0;
    }

    Mesh Mesh::createCube()
    {
        // Unit cube centered at origin
        // 24 unique vertices (per-face normals, UVs). For brevity, a minimal set with flat normals.
        std::vector<float> v = {
            // positions        // normals         // uvs
            // front face
            -0.5f,-0.5f, 0.5f,   0,0,1,   0,0,
             0.5f,-0.5f, 0.5f,   0,0,1,   1,0,
             0.5f, 0.5f, 0.5f,   0,0,1,   1,1,
            -0.5f, 0.5f, 0.5f,   0,0,1,   0,1,
            // back face
            -0.5f,-0.5f,-0.5f,   0,0,-1,  1,0,
             0.5f,-0.5f,-0.5f,   0,0,-1,  0,0,
             0.5f, 0.5f,-0.5f,   0,0,-1,  0,1,
            -0.5f, 0.5f,-0.5f,   0,0,-1,  1,1,
            // left face
            -0.5f,-0.5f,-0.5f,  -1,0,0,   0,0,
            -0.5f,-0.5f, 0.5f,  -1,0,0,   1,0,
            -0.5f, 0.5f, 0.5f,  -1,0,0,   1,1,
            -0.5f, 0.5f,-0.5f,  -1,0,0,   0,1,
            // right face
             0.5f,-0.5f,-0.5f,   1,0,0,   1,0,
             0.5f,-0.5f, 0.5f,   1,0,0,   0,0,
             0.5f, 0.5f, 0.5f,   1,0,0,   0,1,
             0.5f, 0.5f,-0.5f,   1,0,0,   1,1,
            // bottom face
            -0.5f,-0.5f,-0.5f,   0,-1,0,  0,1,
             0.5f,-0.5f,-0.5f,   0,-1,0,  1,1,
             0.5f,-0.5f, 0.5f,   0,-1,0,  1,0,
            -0.5f,-0.5f, 0.5f,   0,-1,0,  0,0,
            // top face
            -0.5f, 0.5f,-0.5f,   0,1,0,   0,0,
             0.5f, 0.5f,-0.5f,   0,1,0,   1,0,
             0.5f, 0.5f, 0.5f,   0,1,0,   1,1,
            -0.5f, 0.5f, 0.5f,   0,1,0,   0,1,
        };
        std::vector<unsigned int> idx = {
            0,1,2, 2,3,0,
            4,5,6, 6,7,4,
            8,9,10, 10,11,8,
            12,13,14, 14,15,12,
            16,17,18, 18,19,16,
            20,21,22, 22,23,20
        };

        Mesh m;
        m.create(v, idx);
        return m;
    }

    Mesh Mesh::createPlane()
    {
        // Simple 2x2 quad in XZ-plane centered at origin
        std::vector<float> v = {
            // positions           // normals      // uvs
            -1.0f, 0.0f, -1.0f,    0,1,0,          0,0,
             1.0f, 0.0f, -1.0f,    0,1,0,          1,0,
             1.0f, 0.0f,  1.0f,    0,1,0,          1,1,
            -1.0f, 0.0f,  1.0f,    0,1,0,          0,1,
        };
        std::vector<unsigned int> idx = { 0,1,2, 2,3,0 };
        Mesh m;
        m.create(v, idx);
        return m;
    }
}


