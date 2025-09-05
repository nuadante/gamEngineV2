#pragma once

namespace engine
{
    class Renderer
    {
    public:
        static void initialize();
        static void beginFrame(int width, int height, float r, float g, float b, float a);
        static void endFrame();
        static void shutdown();
    };
}


