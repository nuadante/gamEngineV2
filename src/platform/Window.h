#pragma once

#include <string>
struct GLFWwindow;

namespace engine
{
    struct WindowProps
    {
        std::string title = "ImGui Basit Proje";
        int width = 1280;
        int height = 720;
        bool vsync = true;
    };

    class Window
    {
    public:
        Window() = default;
        ~Window();

        bool create(const WindowProps& props);
        void destroy();

        void pollEvents();
        void swapBuffers();

        inline GLFWwindow* getNativeHandle() const { return m_window; }
        inline int getWidth() const { return m_width; }
        inline int getHeight() const { return m_height; }
        bool isOpen() const;

        void setVSync(bool enabled);

    private:
        GLFWwindow* m_window = nullptr;
        int m_width = 0;
        int m_height = 0;
        bool m_vsync = true;
    };
}


