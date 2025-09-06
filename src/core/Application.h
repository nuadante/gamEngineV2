#pragma once

#include <memory>
#include <vector>
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
    class PointShadowMap;
    class PostProcess;
    class ParticleSystem;
    class Skybox;
    class InputMap;
    class Physics;
    class SkinnedMesh;
    class Skeleton;
    class Animator;
    struct Animation;
    class Terrain;
    class LuaEngine;
    class AudioEngine;
    class IBL;
    class CascadedShadowMap;

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
        // physics helpers
        void createPhysicsForEntity(int entityIndex);
        void destroyPhysicsForEntity(int entityIndex);
        void rebuildPhysicsFromScene();
        // picking helpers
        void handlePicking();

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
        std::unique_ptr<Shader> m_pbrShader;
        std::unique_ptr<PointShadowMap> m_pointShadowMap;
        std::unique_ptr<Shader> m_pointDepthShader;
        std::unique_ptr<Skybox> m_skybox;
        std::unique_ptr<InputMap> m_inputMap;
        std::unique_ptr<Physics> m_physics;
        std::unique_ptr<PostProcess> m_post;
        std::unique_ptr<ParticleSystem> m_particles;
        // Skinned
        std::unique_ptr<Shader> m_skinShader;
        std::unique_ptr<SkinnedMesh> m_skinMesh;
        std::unique_ptr<Skeleton> m_skinSkeleton;
        std::unique_ptr<Animator> m_skinAnimator;
        std::vector<Animation> m_skinAnimations;
        Texture2D* m_skinDiffuse = nullptr;
        // Terrain
        std::unique_ptr<Terrain> m_terrain;
        char m_heightPath[260] = "";
        char m_splatCtrlPath[260] = "";
        char m_splat0Path[260] = "";
        char m_splat1Path[260] = "";
        char m_splat2Path[260] = "";
        char m_splat3Path[260] = "";
        char m_normalPath[260] = "";
        int m_terrainLOD = 0;
        float m_terrainHeightScale = 20.0f;
        float m_terrainSplatTiling = 16.0f;
        float m_terrainCellWorld = 1.0f;
        // Lua scripting
        std::unique_ptr<LuaEngine> m_lua;
        bool m_luaHotReload = true;
        // Audio
        std::unique_ptr<AudioEngine> m_audio;
        char m_musicPath[260] = "";
        char m_sfxPath[260] = "";
        unsigned int m_musicSrc = 0;
        // IBL
        std::unique_ptr<IBL> m_ibl;
        bool m_useIBL = false;
        char m_hdrPath[260] = "";
        ImVec4 m_clearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        bool m_showDemo = false;
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
        // Post-process
        float m_exposure = 1.0f;
        float m_gamma = 2.2f;
        bool m_fxaa = true;
        // Particles
        bool m_particlesEmit = true;
        float m_particlesRate = 50.0f;
        float m_particlesLifetime = 1.5f;
        float m_particlesSize = 6.0f;
        float m_particlesGravityY = -3.0f;
        float m_particlesColor[3] = {1.0f, 0.6f, 0.2f};
        bool m_particlesAdditive = true;
        // Skinned controls
        char m_skinPath[260] = "";
        int m_skinAnimIndex = 0;
        bool m_skinLoop = true;
        bool m_skinPlaying = false;
        float m_skinSpeed = 1.0f;
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
        // CSM/PCF
        std::unique_ptr<CascadedShadowMap> m_csm;
        bool m_csmEnabled = false;
        int m_cascadeCount = 3; // max 4
        int m_csmSize = 1024;
        float m_cascadeEnds[4] = {5.0f, 20.0f, 60.0f, 100.0f};
        float m_camNear = 0.1f;
        float m_camFar = 100.0f;
        float m_lightRadius = 0.5f; // for PCSS
        bool m_usePCF = true;
        bool m_usePCSS = false;
        int m_pcfKernel = 2; // radius in texels
        float m_pcfRadiusWorld = 0.0f; // optional world radius
        float m_cascadeFade = 0.0f; // not used yet
        float m_cascadeStabilize = 0.0f; // not used yet
        float m_cascadeMatrices[4][16]; // upload helper
        // Point shadow
        bool m_pointShadowEnabled = false;
        float m_pointLightPos[3] = { 2.0f, 4.0f, 2.0f };
        float m_pointLightColor[3] = { 1.0f, 1.0f, 1.0f };
        float m_pointShadowBias = 0.02f;
        float m_pointShadowFar = 25.0f;
        int m_pointShadowSize = 512;
        // Spot light + shadow
        bool m_spotEnabled = false;
        float m_spotPos[3] = { 3.0f, 5.0f, 3.0f };
        float m_spotDir[3] = { -1.0f, -1.0f, -1.0f };
        float m_spotColor[3] = { 1.0f, 1.0f, 1.0f };
        float m_spotInner = 15.0f; // degrees
        float m_spotOuter = 25.0f; // degrees
        float m_spotNear = 0.1f;
        float m_spotFar = 40.0f;
        float m_spotBias = 0.002f;
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

        struct PhysBinding { void* actor; int entityIndex; };
        std::vector<PhysBinding> m_physBindings;
        bool m_drawColliders = true;
        bool m_enablePicking = true;
        bool m_pushOnPick = true;
        float m_pushStrength = 10.0f;
        bool m_pickClickConsumed = false;
        bool m_useCpuPickFallback = true;
        bool m_autoCreateRigidOnPush = true;

        // Model import (Assimp)
        char m_modelPath[260] = "";
        std::vector<std::unique_ptr<Mesh>> m_importedMeshes;
    };
}


