#pragma once

#include <memory>
#include <imgui.h>

struct GLFWwindow;

namespace engine
{
    class Window;
    class Shader;
    class Mesh;
    class Camera;
    class Time;
    class Texture2D;
    class Scene;
    class Transform;
    class ResourceManager;

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
        std::unique_ptr<Shader> m_shader;
        std::unique_ptr<Mesh> m_cube;
        std::unique_ptr<Camera> m_camera;
        std::unique_ptr<Time> m_time;
        std::unique_ptr<Texture2D> m_texture;
        std::unique_ptr<Scene> m_scene;
        std::unique_ptr<Transform> m_cubeTransform;
        std::unique_ptr<ResourceManager> m_resources;
        ImVec4 m_clearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        bool m_showDemo = true;
        bool m_showAnother = false;

        // Lighting/UI parameters
        float m_lightPos[3] = { 3.0f, 3.0f, 3.0f };
        float m_lightColor[3] = { 1.0f, 1.0f, 1.0f };
        float m_albedo[3] = { 1.0f, 0.7f, 0.2f };
        float m_shininess = 64.0f;
        bool m_useTexture = true;
        char m_texPath[260] = "";
    };
}


