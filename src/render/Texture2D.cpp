#include "render/Texture2D.h"

#include <glad/glad.h>

namespace engine
{
    Texture2D::~Texture2D() { destroy(); }

    bool Texture2D::createRGBA8(int width, int height, const std::vector<uint8_t>& pixels)
    {
        m_width = width; m_height = height;
        if (pixels.size() < static_cast<size_t>(width * height * 4)) return false;

        glGenTextures(1, &m_tex);
        glBindTexture(GL_TEXTURE_2D, m_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    }

    bool Texture2D::createCheckerboard(int width, int height, int tileSize)
    {
        std::vector<uint8_t> data(width * height * 4);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                bool odd = ((x / tileSize) + (y / tileSize)) % 2;
                uint8_t c = odd ? 255 : 30;
                size_t i = static_cast<size_t>(y * width + x) * 4;
                data[i + 0] = c;
                data[i + 1] = c;
                data[i + 2] = c;
                data[i + 3] = 255;
            }
        }
        return createRGBA8(width, height, data);
    }

    void Texture2D::bind(int slot) const
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_tex);
    }

    void Texture2D::destroy()
    {
        if (m_tex)
        {
            glDeleteTextures(1, &m_tex);
            m_tex = 0;
        }
    }
}


