#pragma once

#include <cstdint>
#include <vector>

namespace engine
{
    class Texture2D
    {
    public:
        Texture2D() = default;
        ~Texture2D();

        bool createRGBA8(int width, int height, const std::vector<uint8_t>& pixels);
        bool createCheckerboard(int width, int height, int tileSize);
        void bind(int slot) const;
        void destroy();

    private:
        unsigned int m_tex = 0;
        int m_width = 0;
        int m_height = 0;
    };
}


