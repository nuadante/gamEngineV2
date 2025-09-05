#pragma once

#include <vector>

struct GLFWwindow;

namespace engine
{
    class InputManager
    {
    public:
        InputManager();
        ~InputManager();

        void initialize(GLFWwindow* window);
        void shutdown();

        bool isKeyPressed(int key) const;
        bool isMouseButtonPressed(int button) const;
        void getCursorPosition(double& x, double& y) const;

    private:
        static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
        static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
        static void charCallback(GLFWwindow* window, unsigned int c);

        static InputManager* from(GLFWwindow* window);

    private:
        GLFWwindow* m_window = nullptr;
        std::vector<bool> m_keys;
        std::vector<bool> m_mouseButtons;
        double m_cursorX = 0.0;
        double m_cursorY = 0.0;

        static InputManager* s_instance;
    };
}


