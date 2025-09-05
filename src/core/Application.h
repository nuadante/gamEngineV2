#pragma once

#include <memory>
#include <imgui.h>

struct GLFWwindow;

namespace engine
{
    class Window;

    class Application
    {
    public:
        Application();
        ~Application();

        bool initialize();
        int run();
        void shutdown();

    private:
        bool initializeGLFW();
        bool initializeGLAD();
        bool initializeImGui();
        void shutdownImGui();

    private:
        std::unique_ptr<Window> m_window;
        std::unique_ptr<class InputManager> m_input;
        ImVec4 m_clearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        bool m_showDemo = true;
        bool m_showAnother = false;
    };
}


