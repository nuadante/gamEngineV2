#include "platform/Window.h"

#include <GLFW/glfw3.h>

namespace engine
{
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height)
    {
        (void)window;
        // We don't have access to the Window instance here without userdata; skip for now
        // The Application will query framebuffer size each frame as needed
        (void)width;
        (void)height;
    }

    Window::~Window()
    {
        destroy();
    }

    bool Window::create(const WindowProps& props)
    {
        m_width = props.width;
        m_height = props.height;
        m_vsync = props.vsync;

        m_window = glfwCreateWindow(props.width, props.height, props.title.c_str(), nullptr, nullptr);
        if (m_window == nullptr)
        {
            return false;
        }

        glfwMakeContextCurrent(m_window);
        setVSync(m_vsync);

        glfwSetFramebufferSizeCallback(m_window, framebuffer_size_callback);
        return true;
    }

    void Window::destroy()
    {
        if (m_window)
        {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
    }

    void Window::pollEvents()
    {
        glfwPollEvents();
    }

    void Window::swapBuffers()
    {
        if (m_window)
        {
            glfwSwapBuffers(m_window);
        }
    }

    bool Window::isOpen() const
    {
        return m_window != nullptr && !glfwWindowShouldClose(m_window);
    }

    void Window::setVSync(bool enabled)
    {
        m_vsync = enabled;
        glfwSwapInterval(m_vsync ? 1 : 0);
    }
}


