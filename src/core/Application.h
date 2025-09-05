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
    class ShadowMap;
    class Skybox;
    class InputMap;

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
        std::unique_ptr<ShadowMap> m_shadowMap;
        std::unique_ptr<Shader> m_depthShader;
        std::unique_ptr<Skybox> m_skybox;
        std::unique_ptr<InputMap> m_inputMap;
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
        bool m_wireframe = false;
        bool m_vsync = true;
        // Shader reloader
        char m_vsPath[260] = "";
        char m_fsPath[260] = "";
        // Shadows
        bool m_shadowsEnabled = true;
        float m_shadowBias = 0.0015f;
        int m_shadowMapSize = 1024;
        float m_shadowOrthoSize = 10.0f;
        float m_shadowNear = 0.1f;
        float m_shadowFar = 50.0f;
        // Sky
        float m_skyTop[3] = {0.2f, 0.4f, 0.8f};
        float m_skyBottom[3] = {0.9f, 0.9f, 1.0f};
        bool m_orbitMode = false;
        float m_cameraSpeed = 3.0f;
        float m_mouseSensitivity = 0.1f;
        // Rebind state
        bool m_rebindActive = false;
        char m_rebindAxis[32] = "";
        bool m_rebindPositive = true;
        // Gizmo state (0=Translate,1=Rotate,2=Scale), axis (0=X,1=Y,2=Z)
        int m_gizmoOp = 0;
        int m_gizmoAxis = 0;
        float m_gizmoSensitivity = 0.01f;
    };
}


