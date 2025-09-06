#pragma once

#include <memory>
#include <vector>
#include <functional>

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
        void registerPanel(const std::function<void()>& panel);
        void drawPanels();
        void endFrame();
        void renderDrawData();
        void shutdown();

    private:
        std::vector<std::function<void()>> m_panels;
    };
}


