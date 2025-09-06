#include "render/Shader.h"

#include <glad/glad.h>
#include <iostream>

namespace engine
{
    static unsigned int compileStage(unsigned int type, const std::string& source)
    {
        unsigned int shader = glCreateShader(type);
        const char* src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char infoLog[1024];
            glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
            std::cerr << "Shader compile error: " << infoLog << std::endl;
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    Shader::~Shader()
    {
        destroy();
    }

    bool Shader::compileFromSource(const std::string& vertexSrc, const std::string& fragmentSrc)
    {
        unsigned int vs = compileStage(GL_VERTEX_SHADER, vertexSrc);
        if (!vs) return false;
        unsigned int fs = compileStage(GL_FRAGMENT_SHADER, fragmentSrc);
        if (!fs) { glDeleteShader(vs); return false; }

        unsigned int newProgram = glCreateProgram();
        glAttachShader(newProgram, vs);
        glAttachShader(newProgram, fs);
        glLinkProgram(newProgram);

        glDeleteShader(vs);
        glDeleteShader(fs);

        int success;
        glGetProgramiv(newProgram, GL_LINK_STATUS, &success);
        if (!success)
        {
            char infoLog[1024];
            glGetProgramInfoLog(newProgram, 1024, nullptr, infoLog);
            std::cerr << "Program link error: " << infoLog << std::endl;
            glDeleteProgram(newProgram);
            return false;
        }

        // Swap in new program, delete old one
        if (m_program)
        {
            glDeleteProgram(m_program);
        }
        m_program = newProgram;
        return true;
    }

    void Shader::bind() const { glUseProgram(m_program); }
    void Shader::unbind() const { glUseProgram(0); }

    void Shader::destroy()
    {
        if (m_program)
        {
            glDeleteProgram(m_program);
            m_program = 0;
        }
    }

    void Shader::setMat4(const char* name, const float* value) const
    {
        int loc = glGetUniformLocation(m_program, name);
        if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, value);
    }

    void Shader::setVec3(const char* name, float x, float y, float z) const
    {
        int loc = glGetUniformLocation(m_program, name);
        if (loc != -1) glUniform3f(loc, x, y, z);
    }

    void Shader::setMat3(const char* name, const float* value) const
    {
        int loc = glGetUniformLocation(m_program, name);
        if (loc != -1) glUniformMatrix3fv(loc, 1, GL_FALSE, value);
    }

    void Shader::setFloat(const char* name, float v) const
    {
        int loc = glGetUniformLocation(m_program, name);
        if (loc != -1) glUniform1f(loc, v);
    }

    void Shader::setInt(const char* name, int v) const
    {
        int loc = glGetUniformLocation(m_program, name);
        if (loc != -1) glUniform1i(loc, v);
    }
    // already added setMat4Array earlier

    void Shader::setMat4Array(const char* name, const float* value, int count) const
    {
        int loc = glGetUniformLocation(m_program, name);
        if (loc != -1) glUniformMatrix4fv(loc, count, GL_FALSE, value);
    }
}


