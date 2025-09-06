#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <string>

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
        void registerPanel(const char* name, const std::function<void()>& panel, bool* visibleFlag);
        void drawPanels();
        void endFrame();
        void renderDrawData();
        void shutdown();

    private:
        struct Panel { std::string name; std::function<void()> draw; bool* visible; };
        std::vector<Panel> m_panels;
    };
}


