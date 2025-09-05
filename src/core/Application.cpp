#include "core/Application.h"
#include "platform/Window.h"
#include "input/InputManager.h"

#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include "render/Shader.h"
#include "render/Mesh.h"
#include "render/Renderer.h"
#include "render/Camera.h"
#include "render/Texture2D.h"
#include "render/ShadowMap.h"
#include "render/PointShadowMap.h"
#include "render/Skybox.h"
#include "input/InputMap.h"
#include "scene/SceneSerializer.h"
#include "scene/Scene.h"
#include "scene/Transform.h"
#include "core/ResourceManager.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include "core/Time.h"
#include "physics/Physics.h"
#include <PxPhysicsAPI.h>

namespace engine
{
    static glm::vec3 screenToRayDir(double mouseX, double mouseY, int fbWidth, int fbHeight, const glm::mat4& proj, const glm::mat4& view)
    {
        // NDC
        float x = (2.0f * (float)mouseX) / (float)fbWidth - 1.0f;
        float y = 1.0f - (2.0f * (float)mouseY) / (float)fbHeight;
        glm::vec4 rayClip(x, y, -1.0f, 1.0f);
        glm::mat4 invProj = glm::inverse(proj);
        glm::vec4 rayEye = invProj * rayClip;
        rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);
        glm::mat4 invView = glm::inverse(view);
        glm::vec4 rayWorld = invView * rayEye;
        return glm::normalize(glm::vec3(rayWorld));
    }

    void Application::handlePicking()
    {
        if (!m_enablePicking) return;
        if (!m_input) return;
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) { m_pickClickConsumed = false; return; }
        if (!m_input->isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) { m_pickClickConsumed = false; return; }
        if (m_pickClickConsumed) return;
        m_pickClickConsumed = true;

        // get mouse pos and framebuffer size
        double mx, my; m_input->getCursorPosition(mx, my);
        int fbw, fbh; glfwGetFramebufferSize(m_window->getNativeHandle(), &fbw, &fbh);
        int winW, winH; glfwGetWindowSize(m_window->getNativeHandle(), &winW, &winH);
        // convert window coords to framebuffer coords (DPI scaling)
        double fx = mx * (winW > 0 ? (double)fbw / (double)winW : 1.0);
        double fy = my * (winH > 0 ? (double)fbh / (double)winH : 1.0);
        glm::vec3 rayDir = screenToRayDir(fx, fy, fbw, fbh, m_camera->projection(), m_camera->view());
        glm::vec3 rayOrigin = m_camera->position();

        // PhysX raycast
        physx::PxVec3 origin(rayOrigin.x, rayOrigin.y, rayOrigin.z);
        physx::PxVec3 unitDir(rayDir.x, rayDir.y, rayDir.z);
        float maxDist = 1000.0f;
        physx::PxRaycastBuffer hit;
        physx::PxScene* scene = m_physics ? m_physics->scene() : nullptr;
        if (!scene) return;
        bool status = scene->raycast(origin, unitDir, maxDist, hit);
        if (!status || !hit.hasBlock)
        {
            // Fallback: if push is requested and no rigid exists, find closest entity under ray by AABB from mesh
            if (!m_useCpuPickFallback) return;
            int best = -1; float bestT = 1e9f;
            for (int i = 0; i < (int)m_scene->entities().size(); ++i)
            {
                auto& ent = m_scene->entities()[i];
                if (!ent.mesh) continue;
                // approximate with unit cube bounds scaled by transform
                glm::vec3 minB = ent.transform.position - ent.transform.scale * 0.5f;
                glm::vec3 maxB = ent.transform.position + ent.transform.scale * 0.5f;
                // ray-AABB intersect
                glm::vec3 invD = 1.0f / glm::vec3(rayDir);
                glm::vec3 t0 = (minB - rayOrigin) * invD;
                glm::vec3 t1 = (maxB - rayOrigin) * invD;
                glm::vec3 tmin = glm::min(t0, t1);
                glm::vec3 tmax = glm::max(t0, t1);
                float tnear = std::max(std::max(tmin.x, tmin.y), tmin.z);
                float tfar  = std::min(std::min(tmax.x, tmax.y), tmax.z);
                if (tnear <= tfar && tfar > 0.0f && tnear < bestT)
                {
                    bestT = tnear; best = i;
                }
            }
            if (best >= 0)
            {
                m_scene->setSelectedIndex(best);
                if (m_pushOnPick && m_autoCreateRigidOnPush)
                {
                    auto& e = m_scene->entities()[best];
                    e.hasRigidBody = true; e.isStatic = false;
                    createPhysicsForEntity(best);
                    // apply impulse next frame on created body via PhysX raycast retry
                }
            }
            return;
        }

        physx::PxRigidActor* actor = hit.block.actor;
        if (!actor) return;
        // find binding
        int entityIndex = -1;
        for (const auto& b : m_physBindings)
        {
            if ((physx::PxRigidActor*)b.actor == actor)
            {
                entityIndex = b.entityIndex;
                break;
            }
        }
        if (entityIndex >= 0) m_scene->setSelectedIndex(entityIndex);

        if (m_pushOnPick)
        {
            physx::PxRigidDynamic* dyn = actor->is<physx::PxRigidDynamic>();
            if (dyn)
            {
                physx::PxVec3 impulse = unitDir * m_pushStrength;
                dyn->addForce(impulse, physx::PxForceMode::eIMPULSE);
                dyn->wakeUp();
            }
        }
    }
    void Application::createPhysicsForEntity(int entityIndex)
    {
        if (!m_physics) return;
        if (entityIndex < 0 || entityIndex >= (int)m_scene->entities().size()) return;
        auto& e = m_scene->entities()[entityIndex];
        // remove previous binding for this entity if exists
        for (auto it = m_physBindings.begin(); it != m_physBindings.end(); )
        {
            if (it->entityIndex == entityIndex) it = m_physBindings.erase(it); else ++it;
        }
        if (!e.hasRigidBody) return;
        // Create only box dynamic for now
        float hx = e.colliderHalf[0], hy = e.colliderHalf[1], hz = e.colliderHalf[2];
        void* actor = nullptr;
        if (e.isStatic)
        {
            // approximate static by very heavy dynamic disabled: better to add API for static shapes, skipped for brevity
            actor = m_physics->addDynamicBox(e.transform.position.x, e.transform.position.y, e.transform.position.z, hx, hy, hz, 0.0f);
        }
        else
        {
            float density = std::max(0.001f, e.mass);
            actor = m_physics->addDynamicBox(e.transform.position.x, e.transform.position.y, e.transform.position.z, hx, hy, hz, density);
        }
        if (actor)
            m_physBindings.push_back({ actor, entityIndex });
    }

    void Application::destroyPhysicsForEntity(int entityIndex)
    {
        for (auto it = m_physBindings.begin(); it != m_physBindings.end(); )
        {
            if (it->entityIndex == entityIndex)
                it = m_physBindings.erase(it);
            else
                ++it;
        }
    }

    void Application::rebuildPhysicsFromScene()
    {
        m_physBindings.clear();
        for (int i = 0; i < (int)m_scene->entities().size(); ++i)
            createPhysicsForEntity(i);
    }
    static void glfw_error_callback(int error, const char* description)
    {
        std::cerr << "GLFW Error " << error << ": " << description << std::endl;
    }

    Application::Application() = default;
    Application::~Application()
    {
        shutdown();
    }

    bool Application::initialize()
    {
        std::cout << "[App] initialize start" << std::endl;
        if (!initializeGLFW())
        {
            std::cerr << "[App] initializeGLFW failed" << std::endl;
            return false;
        }

        m_window = std::make_unique<Window>();
        if (!m_window->create({}))
        {
            std::cerr << "[App] Window create failed" << std::endl;
            glfwTerminate();
            return false;
        }
        std::cout << "[App] Window created" << std::endl;

        m_input = std::make_unique<InputManager>();
        m_input->initialize(m_window->getNativeHandle());
        std::cout << "[App] Input initialized" << std::endl;

        if (!initializeGLAD())
        {
            std::cerr << "[App] initializeGLAD failed" << std::endl;
            glfwTerminate();
            return false;
        }
        std::cout << "[App] GLAD initialized" << std::endl;

        if (!initializeImGui())
        {
            std::cerr << "[App] initializeImGui failed" << std::endl;
            glfwTerminate();
            return false;
        }
        std::cout << "[App] ImGui initialized" << std::endl;

        // Renderer bootstrap
        int fbw, fbh; glfwGetFramebufferSize(m_window->getNativeHandle(), &fbw, &fbh);
        Renderer::initialize();

        // Resource manager
        m_resources = std::make_unique<ResourceManager>();
        // Input map
        m_inputMap = std::make_unique<InputMap>();
        m_inputMap->bindAxis("MoveForward", { GLFW_KEY_W, GLFW_KEY_S, 1.0f });
        m_inputMap->bindAxis("MoveRight",   { GLFW_KEY_D, GLFW_KEY_A, 1.0f });
        m_inputMap->bindAxis("MoveUp",      { GLFW_KEY_E, GLFW_KEY_Q, 1.0f });

        // Create shader (Phong + texture toggle)
        const char* vs = R"GLSL(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            layout (location = 1) in vec3 aNormal;
            layout (location = 2) in vec2 aUV;
            uniform mat4 u_Model;
            uniform mat4 u_MVP;
            uniform mat3 u_NormalMatrix;
            out vec3 vNormal;
            out vec3 vWorldPos;
            out vec2 vUV;
            void main()
            {
                vec4 worldPos = u_Model * vec4(aPos, 1.0);
                vWorldPos = worldPos.xyz;
                vNormal = normalize(u_NormalMatrix * aNormal);
                vUV = aUV;
                gl_Position = u_MVP * vec4(aPos, 1.0);
            }
        )GLSL";
        const char* fs = R"GLSL(
            #version 330 core
            out vec4 FragColor;
            in vec3 vNormal;
            in vec3 vWorldPos;
            in vec2 vUV;
            uniform vec3 u_CameraPos;
            uniform vec3 u_LightPos;
            uniform vec3 u_LightColor;
            // point light
            uniform vec3 u_PointLightPos;
            uniform vec3 u_PointLightColor;
            uniform float u_PointShadowFar;
            uniform float u_PointShadowBias;
            uniform bool u_PointShadowsEnabled;
            uniform samplerCube u_PointShadowMap;
            uniform vec3 u_Albedo;
            uniform float u_Shininess;
            uniform bool u_UseTexture;
            uniform sampler2D u_AlbedoTex;
            // shadows
            uniform bool u_ShadowsEnabled;
            uniform mat4 u_LightVP;
            uniform sampler2D u_ShadowMap;
            uniform float u_ShadowBias;
            float samplePointShadow(vec3 worldPos)
            {
                vec3 L = worldPos - u_PointLightPos;
                float current = length(L);
                float closest = texture(u_PointShadowMap, normalize(L)).r;
                return (current - u_PointShadowBias > closest) ? 1.0 : 0.0;
            }
            void main()
            {
                vec3 N = normalize(vNormal);
                vec3 L = normalize(u_LightPos - vWorldPos);
                vec3 V = normalize(u_CameraPos - vWorldPos);
                vec3 H = normalize(L + V);
                float diff = max(dot(N, L), 0.0);
                float spec = pow(max(dot(N, H), 0.0), u_Shininess);
                vec3 baseColor = u_UseTexture ? texture(u_AlbedoTex, vUV).rgb : u_Albedo;
                float shadow = 0.0;
                if (u_ShadowsEnabled)
                {
                    vec4 lightClip = u_LightVP * vec4(vWorldPos, 1.0);
                    vec3 projCoords = lightClip.xyz / lightClip.w;
                    projCoords = projCoords * 0.5 + 0.5;
                    if (projCoords.x >= 0.0 && projCoords.x <= 1.0 && projCoords.y >= 0.0 && projCoords.y <= 1.0 && projCoords.z <= 1.0)
                    {
                        float closestDepth = texture(u_ShadowMap, projCoords.xy).r;
                        float currentDepth = projCoords.z;
                        shadow = currentDepth - u_ShadowBias > closestDepth ? 1.0 : 0.0;
                    }
                }
                vec3 color = baseColor * (0.1 + (1.0 - shadow) * diff) + u_LightColor * (1.0 - shadow) * spec;
                // point light
                vec3 Lp = normalize(u_PointLightPos - vWorldPos);
                vec3 Hp = normalize(Lp + V);
                float diffP = max(dot(N, Lp), 0.0);
                float specP = pow(max(dot(N, Hp), 0.0), u_Shininess);
                float shadowP = 0.0;
                if (u_PointShadowsEnabled)
                    shadowP = samplePointShadow(vWorldPos);
                color += baseColor * (1.0 - shadowP) * diffP + u_PointLightColor * (1.0 - shadowP) * specP;
                FragColor = vec4(color, 1.0);
            }
        )GLSL";
        m_shader = std::unique_ptr<Shader>(m_resources->getShaderFromSource("phong_textured", vs, fs));
        if (!m_shader)
            return false;

        // Depth-only shader for shadow pass
        const char* dvs = R"GLSL(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            uniform mat4 u_Model;
            uniform mat4 u_LightVP;
            void main()
            {
                gl_Position = u_LightVP * u_Model * vec4(aPos, 1.0);
            }
        )GLSL";
        const char* dfs = R"GLSL(
            #version 330 core
            void main() {}
        )GLSL";
        m_depthShader = std::make_unique<Shader>();
        if (!m_depthShader->compileFromSource(dvs, dfs))
        {
            std::cerr << "[DepthShader] compile failed" << std::endl;
            return false;
        }
        // Point light depth shader (distance to light in color)
        const char* pvs = R"GLSL(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            uniform mat4 u_Model;
            uniform mat4 u_View;
            uniform mat4 u_Proj;
            out vec3 vWorldPos;
            void main()
            {
                vec4 wp = u_Model * vec4(aPos, 1.0);
                vWorldPos = wp.xyz;
                gl_Position = u_Proj * u_View * wp;
            }
        )GLSL";
        const char* pfs = R"GLSL(
            #version 330 core
            in vec3 vWorldPos;
            out float FragDist;
            uniform vec3 u_LightPos;
            void main()
            {
                FragDist = length(vWorldPos - u_LightPos);
            }
        )GLSL";
        m_pointDepthShader = std::make_unique<Shader>();
        if (!m_pointDepthShader->compileFromSource(pvs, pfs))
        {
            std::cerr << "[PointDepthShader] compile failed" << std::endl;
            return false;
        }

        // Cube mesh
        m_cube = std::unique_ptr<Mesh>(m_resources->getCube("unit_cube"));

        // Camera
        m_camera = std::make_unique<Camera>();
        float aspect = fbw > 0 && fbh > 0 ? (float)fbw / (float)fbh : 16.0f/9.0f;
        m_camera->setPerspective(45.0f * 3.14159265f / 180.0f, aspect, 0.1f, 100.0f);
        m_camera->setView({2.0f, 2.0f, 2.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});

        // Time
        m_time = std::make_unique<Time>();
        m_time->reset();

        // Texture (checkerboard)
        m_texture = std::unique_ptr<Texture2D>(m_resources->getCheckerboard("checker", 256, 256, 32));

        // Shadow map
        m_shadowMap = std::make_unique<ShadowMap>();
        if (!m_shadowMap->create(m_shadowMapSize, m_shadowMapSize))
        {
            std::cerr << "[ShadowMap] create failed" << std::endl;
            return false;
        }
        // Point shadow map (deferred create on first enable)
        m_pointShadowMap = std::make_unique<PointShadowMap>();

        // Skybox
        m_skybox = std::make_unique<Skybox>();
        if (!m_skybox->initialize())
        {
            std::cerr << "[Skybox] init failed" << std::endl;
            return false;
        }

        // Scene setup
        m_scene = std::make_unique<Scene>();
        // Create multiple entities
        int e0 = m_scene->addEntity("Cube 0", m_cube.get(), m_shader.get(), m_texture.get());
        int e1 = m_scene->addEntity("Cube 1", m_cube.get(), m_shader.get(), m_texture.get());
        int e2 = m_scene->addEntity("Cube 2", m_cube.get(), m_shader.get(), m_texture.get());
        // offset positions
        m_scene->entities()[e0].transform.position = { -1.2f, 0.0f, 0.0f };
        m_scene->entities()[e1].transform.position = {  0.0f, 0.0f, 0.0f };
        m_scene->entities()[e2].transform.position = {  1.2f, 0.0f, 0.0f };

        // Physics setup
        m_physics = std::make_unique<Physics>();
        if (m_physics->initialize())
        {
            if (m_physics->createDefaultScene(-9.81f))
            {
                m_physics->addGroundPlane();
                rebuildPhysicsFromScene();
            }
        }

        return true;
    }

    int Application::run()
    {
        if (!initialize())
            return -1;

        std::cout << "[App] Entering main loop" << std::endl;
        while (m_window->isOpen())
        {
            m_input->beginFrame();
            m_window->pollEvents();
            // Shortcuts
            if ((m_input->isKeyPressed(GLFW_KEY_LEFT_CONTROL) || m_input->isKeyPressed(GLFW_KEY_RIGHT_CONTROL)) && m_input->isKeyPressed(GLFW_KEY_S))
            {
                SceneSerializer::save(*m_scene, "scene.json");
                // optionally save physics-specific data is already included
            }
            if ((m_input->isKeyPressed(GLFW_KEY_LEFT_CONTROL) || m_input->isKeyPressed(GLFW_KEY_RIGHT_CONTROL)) && m_input->isKeyPressed(GLFW_KEY_O))
            {
                SceneSerializer::load(*m_scene, "scene.json");
                rebuildPhysicsFromScene();
            }

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            // Gizmo external lib removed; using simple keyboard nudge

            // (Docking devre dışı bırakıldı - imgui opsiyonları ile etkinleştirilebilir)

            if (m_showDemo)
            {
                ImGui::ShowDemoWindow(&m_showDemo);
            }

            {
                ImGui::Begin("Merhaba ImGui!");
                ImGui::Text("Bu basit bir ImGui projesi!");
                ImGui::Checkbox("Demo Pencere", &m_showDemo);
                ImGui::Checkbox("Başka Pencere", &m_showAnother);
                ImGui::ColorEdit3("clear color", (float*)&m_clearColor);
                ImGui::Separator();
                ImGui::Text("Lighting");
                ImGui::DragFloat3("Light Pos", m_lightPos, 0.1f);
                ImGui::ColorEdit3("Light Color", m_lightColor);
                ImGui::Separator();
                ImGui::Text("Material");
                ImGui::ColorEdit3("Albedo", m_albedo);
                ImGui::SliderFloat("Shininess", &m_shininess, 1.0f, 256.0f);
                ImGui::Checkbox("Use Texture", &m_useTexture);
                ImGui::InputText("Tex Path", m_texPath, sizeof(m_texPath));
                if (ImGui::Button("Load Texture"))
                {
                    if (m_texture && m_texPath[0] != '\0')
                    {
                        m_texture->loadFromFile(m_texPath, true);
                    }
                }
                ImGui::Separator();
                ImGui::Checkbox("Wireframe", &m_wireframe);
                ImGui::Checkbox("VSync", &m_vsync);
                ImGui::Checkbox("Draw Colliders", &m_drawColliders);
                ImGui::Checkbox("Enable Picking (LMB)", &m_enablePicking);
                ImGui::Checkbox("Push On Pick", &m_pushOnPick);
                ImGui::Checkbox("Auto Create Rigid On Push", &m_autoCreateRigidOnPush);
                ImGui::SliderFloat("Push Strength", &m_pushStrength, 0.0f, 200.0f);
                ImGui::Separator();
                ImGui::Text("Shadows");
                ImGui::Checkbox("Enable Shadows", &m_shadowsEnabled);
                ImGui::SliderFloat("Bias", &m_shadowBias, 0.0001f, 0.01f, "%.5f");
                ImGui::Separator();
                ImGui::Text("Point Light");
                ImGui::Checkbox("Enable Point Shadow", &m_pointShadowEnabled);
                ImGui::DragFloat3("Point Pos", m_pointLightPos, 0.1f);
                ImGui::ColorEdit3("Point Color", m_pointLightColor);
                ImGui::SliderFloat("Point Far", &m_pointShadowFar, 1.0f, 100.0f);
                ImGui::SliderFloat("Point Bias", &m_pointShadowBias, 0.0001f, 0.05f, "%.4f");
                ImGui::Separator();
                ImGui::Text("Spot Light");
                ImGui::Checkbox("Enable Spot", &m_spotEnabled);
                ImGui::DragFloat3("Spot Pos", m_spotPos, 0.1f);
                ImGui::DragFloat3("Spot Dir", m_spotDir, 0.01f);
                ImGui::ColorEdit3("Spot Color", m_spotColor);
                ImGui::SliderFloat("Inner (deg)", &m_spotInner, 1.0f, 89.0f);
                ImGui::SliderFloat("Outer (deg)", &m_spotOuter, 1.0f, 89.0f);
                ImGui::SliderFloat("Spot Far", &m_spotFar, 1.0f, 100.0f);
                ImGui::SliderFloat("Spot Bias", &m_spotBias, 0.0001f, 0.01f, "%.4f");
                ImGui::Separator();
                ImGui::Text("Skybox");
                ImGui::ColorEdit3("Top", m_skyTop);
                ImGui::ColorEdit3("Bottom", m_skyBottom);
                static bool useCube = false;
                ImGui::Checkbox("Use Cubemap", &useCube);
                if (ImGui::Button("Load Cubemap"))
                {
                    std::vector<std::string> faces = {
                        "right.jpg","left.jpg","top.jpg","bottom.jpg","front.jpg","back.jpg"
                    };
                    if (m_skybox)
                    {
                        if (m_skybox->loadCubemap(faces))
                            m_skybox->setUseCubemap(true);
                    }
                }
                if (m_skybox) m_skybox->setUseCubemap(useCube);
                ImGui::Separator();
                ImGui::Text("Camera");
                ImGui::Checkbox("Orbit Mode (RMB drag + scroll)", &m_orbitMode);
                ImGui::SliderFloat("Speed", &m_cameraSpeed, 0.1f, 20.0f);
                ImGui::SliderFloat("Mouse Sensitivity", &m_mouseSensitivity, 0.01f, 1.0f);
                ImGui::Separator();
                ImGui::Text("Input Mapping");
                auto drawRebind = [&](const char* axisName){
                    auto a = m_inputMap->axis(axisName);
                    ImGui::Text("%s: +%d  -%d", axisName, a.positiveKey, a.negativeKey);
                    ImGui::SameLine();
                    std::string btnP = std::string("Rebind + ( ") + axisName + ")";
                    if (ImGui::Button(btnP.c_str())) { m_rebindActive = true; strncpy_s(m_rebindAxis, axisName, sizeof(m_rebindAxis)-1); m_rebindPositive = true; }
                    ImGui::SameLine();
                    std::string btnN = std::string("Rebind - ( ") + axisName + ")";
                    if (ImGui::Button(btnN.c_str())) { m_rebindActive = true; strncpy_s(m_rebindAxis, axisName, sizeof(m_rebindAxis)-1); m_rebindPositive = false; }
                };
                drawRebind("MoveForward");
                drawRebind("MoveRight");
                drawRebind("MoveUp");
                if (m_rebindActive)
                {
                    ImGui::Text("Press a key to bind %s %s", m_rebindAxis, m_rebindPositive?"(+)":"(-)");
                    for (int k = 32; k <= 348; ++k)
                    {
                        if (m_input->isKeyPressed(k))
                        {
                            m_inputMap->setAxisKey(m_rebindAxis, m_rebindPositive, k);
                            m_rebindActive = false;
                            break;
                        }
                    }
                }
                ImGui::Text("Shader Reloader");
                ImGui::InputText("VS Path", m_vsPath, sizeof(m_vsPath));
                ImGui::InputText("FS Path", m_fsPath, sizeof(m_fsPath));
                if (ImGui::Button("Reload Shader") && m_vsPath[0] != '\0' && m_fsPath[0] != '\0')
                {
                    auto loadFile = [](const char* path) -> std::string {
                        FILE* f = nullptr;
                        fopen_s(&f, path, "rb");
                        if (!f) return {};
                        fseek(f, 0, SEEK_END);
                        long sz = ftell(f);
                        fseek(f, 0, SEEK_SET);
                        std::string s; s.resize(sz);
                        fread(s.data(), 1, sz, f);
                        fclose(f);
                        return s;
                    };
                    std::string vs = loadFile(m_vsPath);
                    std::string fs = loadFile(m_fsPath);
                    if (!vs.empty() && !fs.empty())
                    {
                        if (!m_shader->compileFromSource(vs, fs))
                        {
                            std::cerr << "[Shader] reload failed" << std::endl;
                        }
                    }
                }

                // Hierarchy
                if (ImGui::Begin("Hierarchy"))
                {
                    if (ImGui::Button("Add Cube"))
                    {
                        int idx = (int)m_scene->entities().size();
                        int e = m_scene->addEntity(std::string("Cube ") + std::to_string(idx), m_cube.get(), m_shader.get(), m_texture.get());
                        m_scene->entities()[e].transform.position = { (float)(idx % 5) - 2.0f, 0.0f, (float)(idx / 5) * 1.5f };
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Delete Selected"))
                    {
                        int sel = m_scene->selectedIndex();
                        if (sel >= 0)
                        {
                            destroyPhysicsForEntity(sel);
                            m_scene->removeEntity(sel);
                            // reindex bindings > selected index shift
                            for (auto& b : m_physBindings)
                                if (b.entityIndex > sel) b.entityIndex--;
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Spawn Phys Box"))
                    {
                        int idx = (int)m_scene->entities().size();
                        int e = m_scene->addEntity(std::string("PhysBox ") + std::to_string(idx), m_cube.get(), m_shader.get(), m_texture.get());
                        m_scene->entities()[e].transform.position = { 0.0f, 5.0f, 0.0f };
                        if (m_physics)
                        {
                            void* act = m_physics->addDynamicBox(0.0f, 5.0f, 0.0f, 0.5f, 0.5f, 0.5f, 1.0f);
                            if (act)
                                m_physBindings.push_back({ act, e });
                        }
                    }
                    auto& es = m_scene->entities();
                    for (int i = 0; i < (int)es.size(); ++i)
                    {
                        bool selected = (m_scene->selectedIndex() == i);
                        if (ImGui::Selectable(es[i].name.c_str(), selected))
                        {
                            m_scene->setSelectedIndex(i);
                        }
                        if (selected)
                        {
                            ImGui::SameLine();
                            if (ImGui::Button("Apply Physics##sel"))
                                createPhysicsForEntity(i);
                        }
                    }
                }
                // Save/Load
                static char scenePath[260] = "scene.json";
                ImGui::InputText("Scene Path", scenePath, sizeof(scenePath));
                if (ImGui::Button("Save (Ctrl+S)")) SceneSerializer::save(*m_scene, scenePath);
                ImGui::SameLine();
                if (ImGui::Button("Load (Ctrl+O)")) SceneSerializer::load(*m_scene, scenePath);
                ImGui::End();

                // Inspector
                if (ImGui::Begin("Inspector"))
                {
                    int sel = m_scene->selectedIndex();
                    if (sel >= 0)
                    {
                        auto& ent = m_scene->entities()[sel];
                        char nameBuf[128];
                        memset(nameBuf, 0, sizeof(nameBuf));
                        strncpy_s(nameBuf, ent.name.c_str(), sizeof(nameBuf) - 1);
                        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
                        {
                            ent.name = nameBuf;
                        }
                        auto& t = ent.transform;
                        ImGui::DragFloat3("Position", &t.position.x, 0.01f);
                        ImGui::DragFloat3("Rotation (rad)", &t.rotationEuler.x, 0.01f);
                        ImGui::DragFloat3("Scale", &t.scale.x, 0.01f, 0.01f, 100.0f);
                        ImGui::Separator();
                        ImGui::Text("Material");
                        ImGui::ColorEdit3("Albedo", ent.albedo);
                        ImGui::SliderFloat("Shininess", &ent.shininess, 1.0f, 256.0f);
                        ImGui::Checkbox("Use Texture (entity)", &ent.useTexture);
                        ImGui::Separator();
                        ImGui::Text("Physics");
                        ImGui::Checkbox("Has RigidBody", &ent.hasRigidBody);
                        if (ent.hasRigidBody)
                        {
                            ImGui::Checkbox("Static", &ent.isStatic);
                            if (!ent.isStatic)
                            {
                                ImGui::Checkbox("Kinematic", &ent.isKinematic);
                                ImGui::SliderFloat("Mass", &ent.mass, 0.01f, 1000.0f);
                            }
                            ImGui::SliderFloat("Friction", &ent.friction, 0.0f, 1.0f);
                            ImGui::SliderFloat("Restitution", &ent.restitution, 0.0f, 1.0f);
                            ImGui::DragFloat3("Box Half-Extents", ent.colliderHalf, 0.01f, 0.01f, 10.0f);
                            if (ImGui::Button("Apply Physics (Rebuild)"))
                            {
                                // apply to current selected entity
                                createPhysicsForEntity(sel);
                            }
                        }
                        ImGui::Separator();
                        // Mesh picker
                        static int meshChoice = 0; // 0 Cube, 1 Plane
                        ImGui::Text("Mesh");
                        ImGui::RadioButton("Cube", &meshChoice, 0); ImGui::SameLine();
                        ImGui::RadioButton("Plane", &meshChoice, 1);
                        if (ImGui::Button("Apply Mesh"))
                        {
                            if (meshChoice == 0) ent.mesh = m_resources->getCube("unit_cube");
                            else ent.mesh = m_resources->getPlane("unit_plane");
                        }
                        // Texture load for this entity
                        static char entTexPath[260] = "";
                        ImGui::InputText("Entity Tex Path", entTexPath, sizeof(entTexPath));
                        if (ImGui::Button("Load Entity Texture"))
                        {
                            if (entTexPath[0] != '\0')
                            {
                                Texture2D* t2d = m_resources->getTextureFromFile(entTexPath, true);
                                if (t2d) ent.albedoTex = t2d;
                            }
                        }
                        ImGui::Separator();
                        ImGui::Text("Gizmo");
                        ImGui::RadioButton("Translate (T)", &m_gizmoOp, 0); ImGui::SameLine();
                        ImGui::RadioButton("Rotate (R)", &m_gizmoOp, 1); ImGui::SameLine();
                        ImGui::RadioButton("Scale (S)", &m_gizmoOp, 2);
                        ImGui::RadioButton("X", &m_gizmoAxis, 0); ImGui::SameLine();
                        ImGui::RadioButton("Y", &m_gizmoAxis, 1); ImGui::SameLine();
                        ImGui::RadioButton("Z", &m_gizmoAxis, 2);
                        ImGui::SliderFloat("Gizmo Sens", &m_gizmoSensitivity, 0.001f, 0.1f, "%.3f");
                    }
                }
                ImGui::End();
                ImGui::End();
            }

            if (m_showAnother)
            {
                ImGui::Begin("Başka Pencere", &m_showAnother);
                ImGui::Text("Bu başka bir pencere!");
                if (ImGui::Button("Kapat"))
                    m_showAnother = false;
                ImGui::End();
            }

            // Defer ImGui::Render() until after gizmo manipulation is submitted
            int display_w, display_h;
            glfwGetFramebufferSize(m_window->getNativeHandle(), &display_w, &display_h);
            Renderer::setWireframe(m_wireframe);
            glfwSwapInterval(m_vsync ? 1 : 0);
            Renderer::beginFrame(display_w, display_h, m_clearColor.x * m_clearColor.w, m_clearColor.y * m_clearColor.w, m_clearColor.z * m_clearColor.w, m_clearColor.w);

            // Camera controls
            float dt = m_time->tick();
            double mdx, mdy; m_input->getCursorDelta(mdx, mdy);
            double sdx, sdy; m_input->getScrollDelta(sdx, sdy);
            const float mouseSensitivity = m_mouseSensitivity;
            if (m_orbitMode)
            {
                static float orbitDistance = 5.0f;
                if (m_input->isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT))
                {
                    float yawDelta = (float)(mdx * mouseSensitivity);
                    float pitchDelta = (float)(-mdy * mouseSensitivity);
                    m_camera->addYawPitch(yawDelta, pitchDelta);
                }
                orbitDistance = std::max(0.5f, orbitDistance - (float)sdy * 0.5f);
                // compute orbit position from yaw/pitch
                float yawRad = glm::radians(m_camera->yaw());
                float pitchRad = glm::radians(m_camera->pitch());
                glm::vec3 center(0.0f);
                glm::vec3 eye;
                eye.x = center.x + orbitDistance * cosf(yawRad) * cosf(pitchRad);
                eye.y = center.y + orbitDistance * sinf(pitchRad);
                eye.z = center.z + orbitDistance * sinf(yawRad) * cosf(pitchRad);
                m_camera->setView(eye, center, {0,1,0});
            }
            else
            {
                if (m_input->isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT))
                {
                    m_camera->addYawPitch((float)(mdx * mouseSensitivity), (float)(-mdy * mouseSensitivity));
                }
                glm::vec3 move(0.0f);
                const float speed = m_cameraSpeed;
                auto axis = [&](const char* name) {
                    AxisBinding a = m_inputMap->axis(name);
                    float v = 0.0f;
                    if (m_input->isKeyPressed(a.positiveKey)) v += 1.0f;
                    if (m_input->isKeyPressed(a.negativeKey)) v -= 1.0f;
                    return v * a.scale;
                };
                move.z += axis("MoveForward") * dt;
                move.x += axis("MoveRight") * dt;
                move.y += axis("MoveUp") * dt;
                if (move.x != 0 || move.y != 0 || move.z != 0) m_camera->moveLocal(move);
            }

            // Update cube rotation
            static float angle = 0.0f;
            angle += dt;
            for (size_t i = 0; i < m_scene->entities().size(); ++i)
            {
                m_scene->entities()[i].transform.rotationEuler.y = angle * (1.0f + 0.2f * (float)i);
            }

            // Step physics and sync bound entities
            if (m_physics)
            {
                m_physics->simulate(dt);
                for (const auto& b : m_physBindings)
                {
                    if (b.entityIndex < 0 || b.entityIndex >= (int)m_scene->entities().size()) continue;
                    physx::PxRigidDynamic* body = reinterpret_cast<physx::PxRigidDynamic*>(b.actor);
                    physx::PxTransform p = body->getGlobalPose();
                    auto& t = m_scene->entities()[b.entityIndex].transform;
                    t.position = { p.p.x, p.p.y, p.p.z };
                    float qw = p.q.w, qx = p.q.x, qy = p.q.y, qz = p.q.z;
                    float sinr_cosp = 2.0f * (qw * qx + qy * qz);
                    float cosr_cosp = 1.0f - 2.0f * (qx * qx + qy * qy);
                    float roll = std::atan2(sinr_cosp, cosr_cosp);
                    float sinp = 2.0f * (qw * qy - qz * qx);
                    float pitch = std::abs(sinp) >= 1 ? std::copysign(3.14159265f / 2.0f, sinp) : std::asin(sinp);
                    float siny_cosp = 2.0f * (qw * qz + qx * qy);
                    float cosy_cosp = 1.0f - 2.0f * (qy * qy + qz * qz);
                    float yaw = std::atan2(siny_cosp, cosy_cosp);
                    t.rotationEuler = { pitch, yaw, roll };
                }
                handlePicking();
            }

            // Shadow pass (directional light with orthographic proj)
            glm::mat4 lightView = glm::lookAt(
                glm::vec3(m_lightPos[0], m_lightPos[1], m_lightPos[2]),
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(0,1,0));
            glm::mat4 lightProj = glm::ortho(-m_shadowOrthoSize, m_shadowOrthoSize, -m_shadowOrthoSize, m_shadowOrthoSize, m_shadowNear, m_shadowFar);
            glm::mat4 lightVP = lightProj * lightView;
            if (m_shadowsEnabled)
            {
                m_shadowMap->begin();
                // render depth
                for (const auto& e : m_scene->getEntities())
                {
                    glm::mat4 model = e.transform.modelMatrix();
                    m_depthShader->bind();
                    m_depthShader->setMat4("u_LightVP", &lightVP[0][0]);
                    m_depthShader->setMat4("u_Model", &model[0][0]);
                    e.mesh->draw();
                }
                m_depthShader->unbind();
                m_shadowMap->end(display_w, display_h);
            }

            // Point shadow pass: render 6 faces storing distance in cubemap
            if (m_pointShadowEnabled)
            {
                if (m_pointShadowMap->size() != m_pointShadowSize)
                {
                    m_pointShadowMap->destroy();
                    m_pointShadowMap->create(m_pointShadowSize);
                }
                glm::vec3 lp(m_pointLightPos[0], m_pointLightPos[1], m_pointLightPos[2]);
                glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, m_pointShadowFar);
                glm::vec3 dirs[6] = { {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1} };
                glm::vec3 ups[6]  = { {0,-1,0},{0,-1,0},{0,0,1},{0,0,-1},{0,-1,0},{0,-1,0} };
                for (int f = 0; f < 6; ++f)
                {
                    m_pointShadowMap->beginFace(f);
                    glm::mat4 view = glm::lookAt(lp, lp + dirs[f], ups[f]);
                    for (const auto& e : m_scene->getEntities())
                    {
                        glm::mat4 model = e.transform.modelMatrix();
                        m_pointDepthShader->bind();
                        m_pointDepthShader->setMat4("u_Proj", &proj[0][0]);
                        m_pointDepthShader->setMat4("u_View", &view[0][0]);
                        m_pointDepthShader->setMat4("u_Model", &model[0][0]);
                        m_pointDepthShader->setVec3("u_LightPos", lp.x, lp.y, lp.z);
                        e.mesh->draw();
                    }
                    m_pointDepthShader->unbind();
                }
                m_pointShadowMap->end(display_w, display_h);
            }

            // Spot shadow pass: reuse 2D ShadowMap with perspective proj
            glm::mat4 spotView = glm::lookAt(
                glm::vec3(m_spotPos[0], m_spotPos[1], m_spotPos[2]),
                glm::vec3(m_spotPos[0], m_spotPos[1], m_spotPos[2]) + glm::normalize(glm::vec3(m_spotDir[0], m_spotDir[1], m_spotDir[2])),
                glm::vec3(0,1,0));
            glm::mat4 spotProj = glm::perspective(glm::radians(m_spotOuter * 2.0f), 1.0f, m_spotNear, m_spotFar);
            glm::mat4 spotVP = spotProj * spotView;
            if (m_spotEnabled)
            {
                m_shadowMap->begin();
                for (const auto& e : m_scene->getEntities())
                {
                    glm::mat4 model = e.transform.modelMatrix();
                    m_depthShader->bind();
                    m_depthShader->setMat4("u_LightVP", &spotVP[0][0]);
                    m_depthShader->setMat4("u_Model", &model[0][0]);
                    e.mesh->draw();
                }
                m_depthShader->unbind();
                m_shadowMap->end(display_w, display_h);
            }

            // Render scene
            for (const auto& e : m_scene->getEntities())
            {
                glm::mat4 model = e.transform.modelMatrix();
                glm::mat4 mvp = m_camera->projection() * m_camera->view() * model;
                glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(model)));

                e.shader->bind();
                e.shader->setMat4("u_MVP", &mvp[0][0]);
                e.shader->setMat4("u_Model", &model[0][0]);
                e.shader->setMat3("u_NormalMatrix", &normalMat[0][0]);
                e.shader->setVec3("u_CameraPos", m_camera->position().x, m_camera->position().y, m_camera->position().z);
                e.shader->setVec3("u_LightPos", m_lightPos[0], m_lightPos[1], m_lightPos[2]);
                e.shader->setVec3("u_LightColor", m_lightColor[0], m_lightColor[1], m_lightColor[2]);
                e.shader->setVec3("u_Albedo", e.albedo[0], e.albedo[1], e.albedo[2]);
                e.shader->setFloat("u_Shininess", e.shininess);
                if (e.useTexture && e.albedoTex)
                {
                    e.shader->setInt("u_UseTexture", 1);
                    e.shader->setInt("u_AlbedoTex", 0);
                    e.albedoTex->bind(0);
                }
                else
                {
                    e.shader->setInt("u_UseTexture", 0);
                }
                // shadows
                e.shader->setInt("u_ShadowsEnabled", m_shadowsEnabled ? 1 : 0);
                e.shader->setMat4("u_LightVP", &lightVP[0][0]);
                e.shader->setFloat("u_ShadowBias", m_shadowBias);
                m_shadowMap->bindDepthTexture(1);
                e.shader->setInt("u_ShadowMap", 1);
                // spot
                e.shader->setInt("u_SpotEnabled", m_spotEnabled ? 1 : 0);
                e.shader->setVec3("u_SpotPos", m_spotPos[0], m_spotPos[1], m_spotPos[2]);
                glm::vec3 sdir = glm::normalize(glm::vec3(m_spotDir[0], m_spotDir[1], m_spotDir[2]));
                e.shader->setVec3("u_SpotDir", sdir.x, sdir.y, sdir.z);
                e.shader->setVec3("u_SpotColor", m_spotColor[0], m_spotColor[1], m_spotColor[2]);
                e.shader->setFloat("u_SpotCosInner", cos(glm::radians(m_spotInner)));
                e.shader->setFloat("u_SpotCosOuter", cos(glm::radians(m_spotOuter)));
                e.shader->setMat4("u_SpotVP", &spotVP[0][0]);
                e.shader->setFloat("u_SpotBias", m_spotBias);
                // point light uniforms (sampling to be added in shader later)
                e.shader->setVec3("u_PointLightPos", m_pointLightPos[0], m_pointLightPos[1], m_pointLightPos[2]);
                e.shader->setVec3("u_PointLightColor", m_pointLightColor[0], m_pointLightColor[1], m_pointLightColor[2]);
                e.shader->setFloat("u_PointShadowFar", m_pointShadowFar);
                e.shader->setFloat("u_PointShadowBias", m_pointShadowBias);
                e.shader->setInt("u_PointShadowsEnabled", m_pointShadowEnabled ? 1 : 0);
                m_pointShadowMap->bindCubemap(2);
                e.shader->setInt("u_PointShadowMap", 2);
                e.mesh->draw();
                e.shader->unbind();
            }

            // Debug draw colliders (boxes only)
            if (m_drawColliders && m_physics)
            {
                bool prevWire = m_wireframe;
                Renderer::setWireframe(true);
                for (const auto& b : m_physBindings)
                {
                    physx::PxRigidDynamic* body = reinterpret_cast<physx::PxRigidDynamic*>(b.actor);
                    if (!body) continue;
                    physx::PxU32 numShapes = body->getNbShapes();
                    if (numShapes == 0) continue;
                    std::vector<physx::PxShape*> shapes(numShapes);
                    body->getShapes(shapes.data(), numShapes);
                    for (physx::PxShape* s : shapes)
                    {
                        physx::PxBoxGeometry boxGeo;
                        if (s->getBoxGeometry(boxGeo))
                        {
                            physx::PxTransform aPose = body->getGlobalPose() * s->getLocalPose();
                            glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(aPose.p.x, aPose.p.y, aPose.p.z));
                            glm::quat rq(aPose.q.w, aPose.q.x, aPose.q.y, aPose.q.z);
                            glm::mat4 R = glm::toMat4(rq);
                            glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(boxGeo.halfExtents.x * 2.0f, boxGeo.halfExtents.y * 2.0f, boxGeo.halfExtents.z * 2.0f));
                            glm::mat4 model = T * R * S;
                            glm::mat4 mvp = m_camera->projection() * m_camera->view() * model;
                            glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(model)));
                            m_shader->bind();
                            m_shader->setMat4("u_MVP", &mvp[0][0]);
                            m_shader->setMat4("u_Model", &model[0][0]);
                            m_shader->setMat3("u_NormalMatrix", &normalMat[0][0]);
                            m_shader->setVec3("u_CameraPos", m_camera->position().x, m_camera->position().y, m_camera->position().z);
                            m_shader->setVec3("u_LightPos", m_lightPos[0], m_lightPos[1], m_lightPos[2]);
                            m_shader->setVec3("u_LightColor", 0.0f, 1.0f, 0.0f);
                            m_shader->setVec3("u_Albedo", 0.0f, 1.0f, 0.0f);
                            m_shader->setFloat("u_Shininess", 8.0f);
                            m_shader->setInt("u_UseTexture", 0);
                            m_shader->setInt("u_ShadowsEnabled", 0);
                            m_cube->draw();
                            m_shader->unbind();
                        }
                    }
                }
                Renderer::setWireframe(prevWire);
            }

            // Simple keyboard gizmo
            {
                int selIdx = m_scene->selectedIndex();
                if (selIdx >= 0)
                {
                    auto& ent = m_scene->entities()[selIdx];
                    if (m_input->isKeyPressed(GLFW_KEY_T)) m_gizmoOp = 0;
                    if (m_input->isKeyPressed(GLFW_KEY_R)) m_gizmoOp = 1;
                    if (m_input->isKeyPressed(GLFW_KEY_S)) m_gizmoOp = 2;
                    float delta = (float)m_gizmoSensitivity;
                    if (m_input->isKeyPressed(GLFW_KEY_LEFT))
                    {
                        if (m_gizmoOp == 0) (&ent.transform.position.x)[m_gizmoAxis] -= delta;
                        else if (m_gizmoOp == 1) (&ent.transform.rotationEuler.x)[m_gizmoAxis] -= delta;
                        else (&ent.transform.scale.x)[m_gizmoAxis] *= (1.0f - delta);
                    }
                    if (m_input->isKeyPressed(GLFW_KEY_RIGHT))
                    {
                        if (m_gizmoOp == 0) (&ent.transform.position.x)[m_gizmoAxis] += delta;
                        else if (m_gizmoOp == 1) (&ent.transform.rotationEuler.x)[m_gizmoAxis] += delta;
                        else (&ent.transform.scale.x)[m_gizmoAxis] *= (1.0f + delta);
                    }
                }
            }

            // Draw skybox last
            m_skybox->draw(m_camera->projection(), m_camera->view(), {m_skyTop[0], m_skyTop[1], m_skyTop[2]}, {m_skyBottom[0], m_skyBottom[1], m_skyBottom[2]});

            // Finalize ImGui and render draw data
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            m_window->swapBuffers();
        }

        shutdown();
        std::cout << "[App] Shutdown completed" << std::endl;
        return 0;
    }

    void Application::shutdown()
    {
        shutdownImGui();
        Renderer::shutdown();
        // release physics actors first
        for (auto& b : m_physBindings) b.actor = nullptr;
        m_physBindings.clear();
        m_cube.reset();
        m_shader.reset();
        m_camera.reset();
        if (m_input)
        {
            m_input->shutdown();
            m_input.reset();
        }
        if (m_window)
        {
            m_window->destroy();
            m_window.reset();
        }
        glfwTerminate();
    }

    bool Application::initializeGLFW()
    {
        glfwSetErrorCallback(glfw_error_callback);
        if (!glfwInit())
            return false;
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        return true;
    }

    bool Application::initializeGLAD()
    {
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            std::cerr << "GLAD yuklenemedi!" << std::endl;
            return false;
        }
        return true;
    }

    bool Application::initializeImGui()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(m_window->getNativeHandle(), false);
        ImGui_ImplOpenGL3_Init("#version 330");
        return true;
    }

    void Application::shutdownImGui()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        if (ImGui::GetCurrentContext())
            ImGui::DestroyContext();
    }
}


