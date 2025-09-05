#include "input/InputManager.h"

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>

namespace engine
{
    InputManager* InputManager::s_instance = nullptr;

    InputManager::InputManager() = default;
    InputManager::~InputManager() { shutdown(); }

    void InputManager::initialize(GLFWwindow* window)
    {
        m_window = window;
        m_keys.assign(512, false);
        m_mouseButtons.assign(16, false);

        s_instance = this;
        glfwSetKeyCallback(window, keyCallback);
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetCursorPosCallback(window, cursorPosCallback);
        glfwSetScrollCallback(window, scrollCallback);
        glfwSetCharCallback(window, charCallback);
    }

    void InputManager::shutdown()
    {
        // Keep callbacks as they are; GLFW will clean up on window destroy
        m_window = nullptr;
        s_instance = nullptr;
        m_keys.clear();
        m_mouseButtons.clear();
        m_hasLastCursor = false;
        m_cursorDeltaX = m_cursorDeltaY = 0.0;
    }

    bool InputManager::isKeyPressed(int key) const
    {
        if (key < 0 || key >= static_cast<int>(m_keys.size())) return false;
        return m_keys[key];
    }

    bool InputManager::isMouseButtonPressed(int button) const
    {
        if (button < 0 || button >= static_cast<int>(m_mouseButtons.size())) return false;
        return m_mouseButtons[button];
    }

    void InputManager::getCursorPosition(double& x, double& y) const
    {
        x = m_cursorX;
        y = m_cursorY;
    }

    void InputManager::getCursorDelta(double& dx, double& dy) const
    {
        dx = m_cursorDeltaX;
        dy = m_cursorDeltaY;
    }

    void InputManager::beginFrame()
    {
        m_cursorDeltaX = 0.0;
        m_cursorDeltaY = 0.0;
    }

    InputManager* InputManager::from(GLFWwindow* window)
    {
        (void)window;
        return s_instance;
    }

    void InputManager::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        (void)scancode; (void)mods;
        InputManager* self = from(window);
        if (!self) return;
        if (key >= 0 && key < static_cast<int>(self->m_keys.size()))
        {
            if (action == GLFW_PRESS) self->m_keys[key] = true;
            else if (action == GLFW_RELEASE) self->m_keys[key] = false;
        }
        ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    }

    void InputManager::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
    {
        (void)mods;
        InputManager* self = from(window);
        if (!self) return;
        if (button >= 0 && button < static_cast<int>(self->m_mouseButtons.size()))
        {
            if (action == GLFW_PRESS) self->m_mouseButtons[button] = true;
            else if (action == GLFW_RELEASE) self->m_mouseButtons[button] = false;
        }
        ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    }

    void InputManager::cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
    {
        InputManager* self = from(window);
        if (!self) return;
        self->m_cursorX = xpos;
        self->m_cursorY = ypos;
        if (!self->m_hasLastCursor)
        {
            self->m_lastCursorX = xpos;
            self->m_lastCursorY = ypos;
            self->m_hasLastCursor = true;
        }
        self->m_cursorDeltaX += (xpos - self->m_lastCursorX);
        self->m_cursorDeltaY += (ypos - self->m_lastCursorY);
        self->m_lastCursorX = xpos;
        self->m_lastCursorY = ypos;
        ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
    }

    void InputManager::scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
    {
        (void)xoffset; (void)yoffset;
        ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
    }

    void InputManager::charCallback(GLFWwindow* window, unsigned int c)
    {
        (void)c;
        ImGui_ImplGlfw_CharCallback(window, c);
    }
}


