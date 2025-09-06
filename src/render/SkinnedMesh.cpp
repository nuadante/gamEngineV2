#include "render/SkinnedMesh.h"

#include <glad/glad.h>

namespace engine
{
    SkinnedMesh::~SkinnedMesh() { destroy(); }
    SkinnedMesh::SkinnedMesh(SkinnedMesh&& o) noexcept { *this = std::move(o); }
    SkinnedMesh& SkinnedMesh::operator=(SkinnedMesh&& o) noexcept
    {
        if (this == &o) return *this;
        m_vao = o.m_vao; o.m_vao = 0;
        m_vboPNU = o.m_vboPNU; o.m_vboPNU = 0;
        m_vboIds = o.m_vboIds; o.m_vboIds = 0;
        m_vboW = o.m_vboW; o.m_vboW = 0;
        m_ebo = o.m_ebo; o.m_ebo = 0;
        m_indexCount = o.m_indexCount; o.m_indexCount = 0;
        m_vertexCount = o.m_vertexCount; o.m_vertexCount = 0;
        return *this;
    }

    bool SkinnedMesh::create(const std::vector<float>& vPNU, const std::vector<unsigned int>& boneIds, const std::vector<float>& weights, const std::vector<unsigned int>& indices)
    {
        if (vPNU.empty() || indices.empty() || boneIds.empty() || weights.empty()) return false;
        m_vertexCount = (unsigned int)(vPNU.size() / 8);
        if (boneIds.size() != m_vertexCount * 4) return false;
        if (weights.size() != m_vertexCount * 4) return false;

        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);

        glGenBuffers(1, &m_vboPNU);
        glBindBuffer(GL_ARRAY_BUFFER, m_vboPNU);
        glBufferData(GL_ARRAY_BUFFER, vPNU.size() * sizeof(float), vPNU.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0); // pos
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1); // normal
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2); // uv
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

        glGenBuffers(1, &m_vboIds);
        glBindBuffer(GL_ARRAY_BUFFER, m_vboIds);
        glBufferData(GL_ARRAY_BUFFER, boneIds.size() * sizeof(unsigned int), boneIds.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(3);
        glVertexAttribIPointer(3, 4, GL_UNSIGNED_INT, 4 * sizeof(unsigned int), (void*)0);

        glGenBuffers(1, &m_vboW);
        glBindBuffer(GL_ARRAY_BUFFER, m_vboW);
        glBufferData(GL_ARRAY_BUFFER, weights.size() * sizeof(float), weights.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

        glGenBuffers(1, &m_ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        m_indexCount = (unsigned int)indices.size();

        glBindVertexArray(0);
        return true;
    }

    void SkinnedMesh::draw() const
    {
        glBindVertexArray(m_vao);
        glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    void SkinnedMesh::destroy()
    {
        if (m_ebo) { glDeleteBuffers(1, &m_ebo); m_ebo = 0; }
        if (m_vboW) { glDeleteBuffers(1, &m_vboW); m_vboW = 0; }
        if (m_vboIds) { glDeleteBuffers(1, &m_vboIds); m_vboIds = 0; }
        if (m_vboPNU) { glDeleteBuffers(1, &m_vboPNU); m_vboPNU = 0; }
        if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
        m_indexCount = 0; m_vertexCount = 0;
    }
}


