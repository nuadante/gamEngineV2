#pragma once

#include <string>

namespace engine
{
    class Shader
    {
    public:
        Shader() = default;
        ~Shader();

        bool compileFromSource(const std::string& vertexSrc, const std::string& fragmentSrc);
        void bind() const;
        void unbind() const;
        void destroy();

        unsigned int id() const { return m_program; }

        // Uniform helpers (minimal set)
        void setMat4(const char* name, const float* value) const;
        void setVec3(const char* name, float x, float y, float z) const;
        void setMat3(const char* name, const float* value) const;
        void setFloat(const char* name, float v) const;

    private:
        unsigned int m_program = 0;
    };
}


