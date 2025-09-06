#pragma once

#include <memory>

struct GLFWwindow;

namespace engine
{
    class UIManager
    {
    public:
        UIManager();
        ~UIManager();

        bool initialize(GLFWwindow* window);
        void beginFrame();
        void endFrame();
        void renderDrawData();
        void shutdown();
    };
}


