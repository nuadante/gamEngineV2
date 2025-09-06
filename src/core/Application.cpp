#include "core/Application.h"
#include "platform/Window.h"
#include "input/InputManager.h"

#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include "ui/UIManager.h"
#include "render/Shader.h"
#include "render/Mesh.h"
#include "render/Renderer.h"
#include "render/Camera.h"
#include "render/Texture2D.h"
#include "render/ShadowMap.h"
#include "render/PointShadowMap.h"
#include "render/PostProcess.h"
#include "render/AssimpLoader.h"
#include "render/Material.h"
#include "render/Skybox.h"
#include "render/CascadedShadowMap.h"
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
#include "render/ParticleSystem.h"
#include "render/SkinnedMesh.h"
#include "render/Skeleton.h"
#include "render/Animator.h"
#include "render/Terrain.h"
#include "scripting/LuaEngine.h"
#include "audio/AudioEngine.h"
#include "render/IBL.h"

namespace engine
{
    static void normalizePlane(float p[4])
    {
        float len = sqrtf(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);
        if (len > 0.0f) { p[0]/=len; p[1]/=len; p[2]/=len; p[3]/=len; }
    }

    void Application::computeCameraFrustum(const float* viewProj)
    {
        const float* m = viewProj; // column-major
        // Extract planes from combined matrix (OpenGL clip space)
        // Left:  m3 + m0; Right: m3 - m0; Bottom: m3 + m1; Top: m3 - m1; Near: m3 + m2; Far: m3 - m2
        float L[4] = { m[12]+m[0],  m[13]+m[1],  m[14]+m[2],  m[15]+m[3] };
        float R[4] = { m[12]-m[0],  m[13]-m[1],  m[14]-m[2],  m[15]-m[3] };
        float B[4] = { m[12]+m[4],  m[13]+m[5],  m[14]+m[6],  m[15]+m[7] };
        float T[4] = { m[12]-m[4],  m[13]-m[5],  m[14]-m[6],  m[15]-m[7] };
        float N[4] = { m[12]+m[8],  m[13]+m[9],  m[14]+m[10], m[15]+m[11] };
        float F[4] = { m[12]-m[8],  m[13]-m[9],  m[14]-m[10], m[15]-m[11] };
        normalizePlane(L); normalizePlane(R); normalizePlane(B); normalizePlane(T); normalizePlane(N); normalizePlane(F);
        memcpy(m_frustumPlanes[0], L, sizeof(L));
        memcpy(m_frustumPlanes[1], R, sizeof(R));
        memcpy(m_frustumPlanes[2], B, sizeof(B));
        memcpy(m_frustumPlanes[3], T, sizeof(T));
        memcpy(m_frustumPlanes[4], N, sizeof(N));
        memcpy(m_frustumPlanes[5], F, sizeof(F));
    }

    bool Application::sphereInFrustum(const float center[3], float radius) const
    {
        for (int i=0;i<6;++i)
        {
            const float* p = m_frustumPlanes[i];
            float d = p[0]*center[0] + p[1]*center[1] + p[2]*center[2] + p[3];
            if (d < -radius) return false;
        }
        return true;
    }
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

        m_ui = std::make_unique<UIManager>();
        if (!m_ui->initialize(m_window->getNativeHandle()))
        {
            std::cerr << "[App] initializeImGui failed" << std::endl;
            glfwTerminate();
            return false;
        }
        std::cout << "[App] ImGui initialized" << std::endl;

        // Renderer bootstrap
        int fbw, fbh; glfwGetFramebufferSize(m_window->getNativeHandle(), &fbw, &fbh);
        Renderer::initialize();
        m_post = std::make_unique<PostProcess>();
        m_post->create(fbw, fbh);
        m_particles = std::make_unique<ParticleSystem>();
        m_particles->initialize(2000);

        // Lua scripting
        m_lua = std::make_unique<LuaEngine>();
        m_lua->initialize();
        m_lua->setHotReloadEnabled(true);

        // Audio
        m_audio = std::make_unique<AudioEngine>();
        m_audio->initialize();

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
            // CSM + PCF/PCSS
            uniform int u_UseCSM;
            uniform int u_CascadeCount;
            uniform mat4 u_CascadeVP[4];
            uniform sampler2D u_CascadeMap[4];
            uniform int u_PCFKernel; // texel radius
            uniform float u_ShadowMapSize;
            uniform int u_UsePCSS;
            uniform float u_LightRadius;

            bool inside01(vec3 p){ return p.x>=0.0 && p.x<=1.0 && p.y>=0.0 && p.y<=1.0 && p.z<=1.0; }
            float sampleShadowMap(sampler2D map, vec3 projCoords)
            {
                float current = projCoords.z;
                float texel = 1.0 / max(u_ShadowMapSize, 1.0);
                int r = max(u_PCFKernel, 0);
                if (u_UsePCSS==1)
                {
                    float scale = 1.0 + current * u_LightRadius;
                    r = int(float(r) * scale);
                }
                float occl = 0.0;
                int count = 0;
                for (int x=-r; x<=r; ++x)
                for (int y=-r; y<=r; ++y)
                {
                    vec2 uv = projCoords.xy + vec2(x,y) * texel;
                    float closest = texture(map, uv).r;
                    occl += (current - u_ShadowBias > closest) ? 1.0 : 0.0;
                    count++;
                }
                return count>0 ? (occl/float(count)) : 0.0;
            }
            float computeShadow(vec3 worldPos)
            {
                if (u_UseCSM==0)
                {
                    vec4 clip = u_LightVP * vec4(worldPos,1.0);
                    vec3 proj = clip.xyz / clip.w; proj = proj*0.5+0.5;
                    if (!inside01(proj)) return 0.0;
                    return sampleShadowMap(u_ShadowMap, proj);
                }
                for (int i=0;i<u_CascadeCount;i++)
                {
                    vec4 clip = u_CascadeVP[i] * vec4(worldPos,1.0);
                    vec3 proj = clip.xyz / max(clip.w, 1e-6); proj = proj*0.5+0.5;
                    if (inside01(proj))
                        return sampleShadowMap(u_CascadeMap[i], proj);
                }
                return 0.0;
            }
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
                float shadow = (u_ShadowsEnabled) ? computeShadow(vWorldPos) : 0.0;
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
        // Skinning shader (Phong + texture)
        const char* skVS = R"GLSL(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            layout (location = 1) in vec3 aNormal;
            layout (location = 2) in vec2 aUV;
            layout (location = 3) in uvec4 aBoneIds;
            layout (location = 4) in vec4 aWeights;
            uniform mat4 u_Model;
            uniform mat4 u_VP;
            uniform mat3 u_NormalMatrix;
            const int MAX_BONES = 128;
            uniform mat4 u_Bones[MAX_BONES];
            out vec3 vNormal;
            out vec3 vWorldPos;
            out vec2 vUV;
            void main(){
                mat4 skin = aWeights.x * u_Bones[aBoneIds.x] + aWeights.y * u_Bones[aBoneIds.y] + aWeights.z * u_Bones[aBoneIds.z] + aWeights.w * u_Bones[aBoneIds.w];
                vec4 wp = u_Model * skin * vec4(aPos,1.0);
                vWorldPos = wp.xyz;
                vNormal = normalize(u_NormalMatrix * mat3(u_Model * skin) * aNormal);
                vUV = aUV;
                gl_Position = u_VP * wp;
            }
        )GLSL";
        const char* skFS = R"GLSL(
            #version 330 core
            out vec4 FragColor;
            in vec3 vNormal; in vec3 vWorldPos; in vec2 vUV;
            uniform vec3 u_CameraPos;
            uniform vec3 u_LightPos;
            uniform vec3 u_LightColor;
            uniform vec3 u_Albedo; uniform float u_Shininess; uniform bool u_UseTexture; uniform sampler2D u_AlbedoTex;
            void main(){
                vec3 N = normalize(vNormal);
                vec3 L = normalize(u_LightPos - vWorldPos);
                vec3 V = normalize(u_CameraPos - vWorldPos);
                vec3 H = normalize(L + V);
                float diff = max(dot(N, L), 0.0);
                float spec = pow(max(dot(N, H), 0.0), u_Shininess);
                vec3 base = u_UseTexture ? texture(u_AlbedoTex, vUV).rgb : u_Albedo;
                vec3 color = base * (0.1 + diff) + u_LightColor * spec;
                FragColor = vec4(color, 1.0);
            }
        )GLSL";
        m_skinShader = std::make_unique<Shader>();
        if (!m_skinShader->compileFromSource(skVS, skFS))
        {
            std::cerr << "[SkinShader] compile failed" << std::endl;
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

        // Terrain
        m_terrain = std::make_unique<Terrain>();
        m_terrain->initialize(257); // 257x257 grid
        m_terrain->setParams(m_terrainHeightScale, m_terrainSplatTiling, m_terrainCellWorld);

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

        // PBR shader (Cook-Torrance GGX, no normal mapping for now)
        const char* pbrVS = R"GLSL(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            layout (location = 1) in vec3 aNormal;
            layout (location = 2) in vec2 aUV;
            uniform mat4 u_Model;
            uniform mat4 u_MVP;
            uniform mat3 u_NormalMatrix;
            out vec3 vN; out vec3 vW; out vec2 vUV;
            void main(){ vec4 w = u_Model * vec4(aPos,1.0); vW=w.xyz; vN=normalize(u_NormalMatrix*aNormal); vUV=aUV; gl_Position=u_MVP*vec4(aPos,1.0);} 
        )GLSL";
        const char* pbrFS = R"GLSL(
            #version 330 core
            out vec4 FragColor;
            in vec3 vN; in vec3 vW; in vec2 vUV;
            uniform vec3 u_Cam;
            uniform vec3 u_LightPos;
            uniform vec3 u_Albedo;
            uniform float u_Metallic; uniform float u_Roughness; uniform float u_AO;
            uniform bool u_UseAlbedoTex; uniform sampler2D u_AlbedoTex;
            uniform bool u_UseMetalTex; uniform sampler2D u_MetalTex;
            uniform bool u_UseRoughTex; uniform sampler2D u_RoughTex;
            uniform bool u_UseAOTex; uniform sampler2D u_AOTex;
            uniform bool u_UseNormalMap; uniform sampler2D u_NormalTex;
            uniform bool u_UseIBL; uniform samplerCube u_IrradianceMap; uniform samplerCube u_PrefilterMap; uniform sampler2D u_BRDFLUT;
            // Shadows (CSM + PCF/PCSS)
            uniform bool u_ShadowsEnabled;
            uniform int u_UseCSM; uniform int u_CascadeCount; uniform mat4 u_CascadeVP[4]; uniform sampler2D u_CascadeMap[4];
            uniform mat4 u_LightVP; uniform sampler2D u_ShadowMap;
            uniform float u_ShadowBias; uniform int u_PCFKernel; uniform float u_ShadowMapSize; uniform int u_UsePCSS; uniform float u_LightRadius;
            bool inside01(vec3 p){ return p.x>=0.0 && p.x<=1.0 && p.y>=0.0 && p.y<=1.0 && p.z<=1.0; }
            float sampleShadowMap(sampler2D map, vec3 projCoords){
              float current = projCoords.z; float texel = 1.0/max(u_ShadowMapSize,1.0); int r=max(u_PCFKernel,0);
              if (u_UsePCSS==1){ float scale = 1.0 + current * u_LightRadius; r = int(float(r)*scale);} float occl=0.0; int cnt=0;
              for(int x=-r;x<=r;++x) for(int y=-r;y<=r;++y){ vec2 uv = projCoords.xy + vec2(x,y)*texel; float closest = texture(map, uv).r; occl += (current - u_ShadowBias > closest) ? 1.0 : 0.0; cnt++; }
              return cnt>0? occl/float(cnt) : 0.0; }
            float computeShadow(vec3 worldPos){ if (!u_ShadowsEnabled) return 0.0; if (u_UseCSM==0){ vec4 clip=u_LightVP*vec4(worldPos,1.0); vec3 proj=clip.xyz/clip.w; proj=proj*0.5+0.5; if(!inside01(proj)) return 0.0; return sampleShadowMap(u_ShadowMap, proj);} for(int i=0;i<u_CascadeCount;i++){ vec4 clip=u_CascadeVP[i]*vec4(worldPos,1.0); vec3 proj=clip.xyz/max(clip.w,1e-6); proj=proj*0.5+0.5; if(inside01(proj)) return sampleShadowMap(u_CascadeMap[i], proj);} return 0.0; }
            float DistributionGGX(vec3 N, vec3 H, float a){ float a2=a*a; float NdotH=max(dot(N,H),0.0); float NdotH2=NdotH*NdotH; float denom=(NdotH2*(a2-1.0)+1.0); return a2/(3.14159265*denom*denom); }
            float GeometrySchlickGGX(float NdotV, float k){ return NdotV/(NdotV*(1.0-k)+k); }
            float GeometrySmith(vec3 N, vec3 V, vec3 L, float k){ float NdotV=max(dot(N,V),0.0); float NdotL=max(dot(N,L),0.0); float g1=GeometrySchlickGGX(NdotV,k); float g2=GeometrySchlickGGX(NdotL,k); return g1*g2; }
            vec3 fresnelSchlick(float cosTheta, vec3 F0){ return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0); }
            void main(){
              vec3 N=normalize(vN);
              if (u_UseNormalMap){
                vec3 dp1 = dFdx(vW);
                vec3 dp2 = dFdy(vW);
                vec2 duv1 = dFdx(vUV);
                vec2 duv2 = dFdy(vUV);
                vec3 T = normalize(dp1*duv2.y - dp2*duv1.y);
                vec3 B = normalize(cross(N, T));
                mat3 TBN = mat3(T, B, N);
                vec3 nTex = texture(u_NormalTex, vUV).xyz * 2.0 - 1.0;
                N = normalize(TBN * nTex);
              }
              vec3 V=normalize(u_Cam - vW);
              vec3 L=normalize(u_LightPos - vW);
              vec3 H=normalize(V+L);
              vec3 base = u_UseAlbedoTex? texture(u_AlbedoTex, vUV).rgb : u_Albedo;
              float metallic = u_UseMetalTex? texture(u_MetalTex, vUV).r : u_Metallic;
              float roughness = clamp(u_UseRoughTex? texture(u_RoughTex, vUV).r : u_Roughness, 0.04, 1.0);
              float ao = u_UseAOTex? texture(u_AOTex, vUV).r : u_AO;
              vec3 F0 = mix(vec3(0.04), base, metallic);
              float NDF = DistributionGGX(N,H, roughness*roughness);
              float G   = GeometrySmith(N,V,L, (roughness+1.0)*(roughness+1.0)/8.0);
              vec3  F   = fresnelSchlick(max(dot(H,V),0.0), F0);
              vec3 kS = F; vec3 kD = (vec3(1.0)-kS) * (1.0 - metallic);
              float NdotL = max(dot(N,L),0.0);
              vec3 spec = (NDF*G*F) / max(4.0*max(dot(N,V),0.0)*NdotL, 0.001);
              float shadow = computeShadow(vW);
              vec3 Lo = (kD*base/3.14159265 + spec) * NdotL * (1.0 - shadow);
              vec3 ambient;
              if (u_UseIBL){
                vec3 irradiance = texture(u_IrradianceMap, N).rgb;
                vec3 diffuse = irradiance * base;
                vec3 R = reflect(-V, N);
                const float MAX_LOD = 4.0;
                vec3 prefiltered = textureLod(u_PrefilterMap, R, roughness * MAX_LOD).rgb;
                vec2 brdf = texture(u_BRDFLUT, vec2(max(dot(N,V),0.0), roughness)).rg;
                vec3 specIBL = prefiltered * (F * brdf.x + brdf.y);
                ambient = (kD * diffuse + specIBL) * ao;
              } else {
                ambient = vec3(0.03) * base * ao;
              }
              FragColor = vec4(ambient + Lo, 1.0);
            }
        )GLSL";
        m_pbrShader = std::make_unique<Shader>();
        if (!m_pbrShader->compileFromSource(pbrVS, pbrFS))
        {
            std::cerr << "[PBR] compile failed" << std::endl;
        }

        // Additional UI Panels
        if (m_ui)
        {
            // General panel
            m_ui->registerPanel([this]()
            {
                if (ImGui::Begin("General"))
                {
                    ImGui::ColorEdit3("Clear Color", (float*)&m_clearColor);
                    ImGui::Checkbox("VSync", &m_vsync);
                    ImGui::Checkbox("Wireframe", &m_wireframe);
                }
                ImGui::End();
            });

            // Lighting & Shadows
            m_ui->registerPanel([this]()
            {
                if (ImGui::Begin("Lighting & Shadows"))
                {
                    ImGui::Text("Lighting");
                    ImGui::DragFloat3("Light Pos", m_lightPos, 0.1f);
                    ImGui::ColorEdit3("Light Color", m_lightColor);
                    ImGui::Separator();
                    ImGui::Text("Shadows");
                    ImGui::Checkbox("Enable Shadows", &m_shadowsEnabled);
                    ImGui::SliderFloat("Bias", &m_shadowBias, 0.0001f, 0.01f, "%.5f");
                    ImGui::Checkbox("Enable CSM", &m_csmEnabled);
                    ImGui::SliderInt("Cascade Count", &m_cascadeCount, 1, 4);
                    ImGui::SliderInt("CSM Size", &m_csmSize, 256, 4096);
                    ImGui::DragFloat4("Cascade Ends", m_cascadeEnds, 0.5f, 0.1f, 500.0f);
                    ImGui::Checkbox("PCF", &m_usePCF); ImGui::SameLine(); ImGui::Checkbox("PCSS", &m_usePCSS);
                    ImGui::SliderInt("PCF Kernel", &m_pcfKernel, 1, 4);
                    ImGui::SliderFloat("Light Radius", &m_lightRadius, 0.0f, 2.0f);
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
                }
                ImGui::End();
            });

            // Post-process & Performance
            m_ui->registerPanel([this]()
            {
                if (ImGui::Begin("Post-process & Performance"))
                {
                    ImGui::Text("Post-process");
                    ImGui::SliderFloat("Exposure", &m_exposure, 0.1f, 5.0f);
                    ImGui::SliderFloat("Gamma", &m_gamma, 1.0f, 2.6f);
                    ImGui::Checkbox("FXAA", &m_fxaa);
                    ImGui::Separator();
                    ImGui::Text("Bloom");
                    ImGui::Checkbox("Bloom Enabled", &m_bloomEnabled);
                    ImGui::SliderFloat("Threshold", &m_bloomThreshold, 0.0f, 5.0f);
                    ImGui::SliderFloat("Intensity", &m_bloomIntensity, 0.0f, 2.0f);
                    ImGui::SliderInt("Iterations", &m_bloomIterations, 1, 10);
                    ImGui::Separator();
                    ImGui::Text("SSAO");
                    ImGui::Checkbox("SSAO Enabled", &m_ssaoEnabled);
                    ImGui::SliderFloat("Radius", &m_ssaoRadius, 0.05f, 2.0f);
                    ImGui::SliderFloat("Bias", &m_ssaoBias, 0.0f, 0.1f, "%.3f");
                    ImGui::SliderFloat("Power", &m_ssaoPower, 0.1f, 3.0f);
                    ImGui::Separator();
                    ImGui::Text("TAA");
                    ImGui::Checkbox("TAA Enabled", &m_taaEnabled);
                    ImGui::SliderFloat("TAA Alpha", &m_taaAlpha, 0.02f, 0.3f);
                    ImGui::Separator();
                    ImGui::Text("Performance");
                    ImGui::Checkbox("Frustum Culling", &m_frustumCulling);
                    ImGui::Checkbox("Instancing (same Mesh)", &m_useInstancing);
                    ImGui::Checkbox("Draw Colliders", &m_drawColliders);
                }
                ImGui::End();
            });

            // Sky & IBL
            m_ui->registerPanel([this]()
            {
                if (ImGui::Begin("Sky & IBL"))
                {
                    ImGui::Text("Skybox");
                    ImGui::ColorEdit3("Top", m_skyTop);
                    ImGui::ColorEdit3("Bottom", m_skyBottom);
                    static bool useCube = false;
                    ImGui::Checkbox("Use Cubemap", &useCube);
                    if (ImGui::Button("Load Cubemap"))
                    {
                        std::vector<std::string> faces = {"right.jpg","left.jpg","top.jpg","bottom.jpg","front.jpg","back.jpg"};
                        if (m_skybox)
                        {
                            if (m_skybox->loadCubemap(faces))
                                m_skybox->setUseCubemap(true);
                        }
                    }
                    if (m_skybox) m_skybox->setUseCubemap(useCube);
                    ImGui::Separator();
                    ImGui::Text("IBL");
                    ImGui::Checkbox("Use IBL", &m_useIBL);
                    ImGui::InputText("HDR Path", m_hdrPath, sizeof(m_hdrPath));
                    if (ImGui::Button("Load HDR"))
                    {
                        if (!m_ibl) m_ibl = std::make_unique<IBL>();
                        if (m_hdrPath[0]) m_useIBL = m_ibl->createFromHDR(m_hdrPath);
                    }
                }
                ImGui::End();
            });

            // Terrain
            m_ui->registerPanel([this]()
            {
                if (ImGui::Begin("Terrain"))
                {
                    ImGui::InputText("Heightmap", m_heightPath, sizeof(m_heightPath));
                    ImGui::InputText("SplatCtrl (RGBA)", m_splatCtrlPath, sizeof(m_splatCtrlPath));
                    ImGui::InputText("Splat0", m_splat0Path, sizeof(m_splat0Path));
                    ImGui::InputText("Splat1", m_splat1Path, sizeof(m_splat1Path));
                    ImGui::InputText("Splat2", m_splat2Path, sizeof(m_splat2Path));
                    ImGui::InputText("Splat3", m_splat3Path, sizeof(m_splat3Path));
                    ImGui::InputText("NormalMap", m_normalPath, sizeof(m_normalPath));
                    if (ImGui::Button("Load Terrain Textures"))
                    {
                        if (m_terrain)
                        {
                            if (m_heightPath[0]) m_terrain->setHeightmap(m_resources->getTextureFromFile(m_heightPath, false));
                            if (m_splatCtrlPath[0]) m_terrain->setSplatControl(m_resources->getTextureFromFile(m_splatCtrlPath, false));
                            if (m_splat0Path[0]) m_terrain->setSplatTexture(0, m_resources->getTextureFromFile(m_splat0Path, false));
                            if (m_splat1Path[0]) m_terrain->setSplatTexture(1, m_resources->getTextureFromFile(m_splat1Path, false));
                            if (m_splat2Path[0]) m_terrain->setSplatTexture(2, m_resources->getTextureFromFile(m_splat2Path, false));
                            if (m_splat3Path[0]) m_terrain->setSplatTexture(3, m_resources->getTextureFromFile(m_splat3Path, false));
                            if (m_normalPath[0]) m_terrain->setNormalMap(m_resources->getTextureFromFile(m_normalPath, false));
                        }
                    }
                    ImGui::SliderInt("LOD", &m_terrainLOD, 0, 2);
                    ImGui::SliderFloat("Height Scale", &m_terrainHeightScale, 1.0f, 200.0f);
                    ImGui::SliderFloat("Splat Tiling", &m_terrainSplatTiling, 1.0f, 64.0f);
                    ImGui::SliderFloat("Cell World Size", &m_terrainCellWorld, 0.25f, 8.0f);
                    if (m_terrain)
                    {
                        m_terrain->setLOD(m_terrainLOD);
                        m_terrain->setParams(m_terrainHeightScale, m_terrainSplatTiling, m_terrainCellWorld);
                    }
                }
                ImGui::End();
            });

            // Scripting (Lua)
            m_ui->registerPanel([this]()
            {
                if (ImGui::Begin("Scripting (Lua)"))
                {
                    ImGui::Checkbox("Hot Reload", &m_luaHotReload);
                    if (m_lua) m_lua->setHotReloadEnabled(m_luaHotReload);
                    if (ImGui::Button("Load Script (Selected Entity)"))
                    {
                        int sel = m_scene->selectedIndex();
                        if (sel >= 0)
                        {
                            auto& e = m_scene->entities()[sel];
                            if (!e.scriptPath.empty() && m_lua)
                            {
                                m_lua->loadScript(e.scriptPath);
                                m_lua->bindEntity(sel, &e);
                                e.scriptEnabled = true;
                            }
                        }
                    }
                }
                ImGui::End();
            });

            // Audio
            m_ui->registerPanel([this]()
            {
                if (ImGui::Begin("Audio"))
                {
                    ImGui::InputText("Music (OGG/WAV)", m_musicPath, sizeof(m_musicPath));
                    ImGui::SameLine();
                    if (ImGui::Button("Play Music"))
                    {
                        if (m_audio && m_musicPath[0]) { auto buf = m_audio->loadSound(m_musicPath); if (buf) m_musicSrc = m_audio->play(buf, 0.6f, true); }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Stop Music"))
                    {
                        if (m_audio && m_musicSrc) { m_audio->stop(m_musicSrc); m_musicSrc = 0; }
                    }
                    ImGui::InputText("SFX (OGG/WAV)", m_sfxPath, sizeof(m_sfxPath));
                    if (ImGui::Button("Play SFX"))
                    {
                        if (m_audio && m_sfxPath[0]) { auto buf = m_audio->loadSound(m_sfxPath); if (buf) m_audio->play3D(buf, 0.0f, 0.0f, 0.0f, 1.0f, false); }
                    }
                }
                ImGui::End();
            });

            // Input Mapping
            m_ui->registerPanel([this]()
            {
                if (ImGui::Begin("Input Mapping"))
                {
                    auto drawRebind = [&](const char* axisName){
                        auto a = m_inputMap->axis(axisName);
                        ImGui::Text("%s: +%d  -%d", axisName, a.positiveKey, a.negativeKey);
                        ImGui::SameLine();
                        std::string btnP = std::string("Rebind + (") + axisName + ")";
                        if (ImGui::Button(btnP.c_str())) { m_rebindActive = true; strncpy_s(m_rebindAxis, axisName, sizeof(m_rebindAxis)-1); m_rebindPositive = true; }
                        ImGui::SameLine();
                        std::string btnN = std::string("Rebind - (") + axisName + ")";
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
                }
                ImGui::End();
            });

            // Import & Animation
            m_ui->registerPanel([this]()
            {
                if (ImGui::Begin("Import & Animation"))
                {
                    ImGui::Text("Model Import (OBJ/FBX/GLTF)");
                    ImGui::InputText("Model Path", m_modelPath, sizeof(m_modelPath));
                    if (ImGui::Button("Import Model") && m_modelPath[0] != '\0')
                    {
                        std::vector<ImportedMesh> ims;
                        if (AssimpLoader::loadModel(m_resources.get(), m_modelPath, ims, true))
                        {
                            for (auto& im : ims)
                            {
                                std::unique_ptr<Mesh> owned(im.mesh);
                                int e = m_scene->addEntity(im.name.empty()?"Imported":im.name, owned.get(), m_shader.get(), im.diffuse);
                                m_scene->entities()[e].useTexture = (im.diffuse != nullptr);
                                if (im.metal || im.rough || im.ao || im.normal)
                                {
                                    auto& ent = m_scene->entities()[e];
                                    ent.usePBR = true;
                                    if (im.metal) ent.metallicTex = im.metal;
                                    if (im.rough) ent.roughnessTex = im.rough;
                                    if (im.ao) ent.aoTex = im.ao;
                                    if (im.normal) ent.normalTex = im.normal;
                                }
                                m_importedMeshes.push_back(std::move(owned));
                            }
                        }
                    }
                    ImGui::Separator();
                    ImGui::Text("Skinned Import (GLTF/FBX)");
                    ImGui::InputText("Skinned Path", m_skinPath, sizeof(m_skinPath));
                    if (ImGui::Button("Import Skinned") && m_skinPath[0] != '\0')
                    {
                        ImportedSkinned isk;
                        if (AssimpLoader::loadSkinned(m_resources.get(), m_skinPath, isk, true))
                        {
                            m_skinMesh.reset(isk.mesh);
                            m_skinSkeleton.reset(isk.skeleton);
                            m_skinDiffuse = isk.diffuse;
                            m_skinAnimations = isk.animations ? *isk.animations : std::vector<Animation>();
                            delete isk.animations;
                            if (!m_skinAnimator) m_skinAnimator = std::make_unique<Animator>();
                            if (!m_skinAnimations.empty())
                            {
                                m_skinAnimIndex = 0; m_skinPlaying = true; m_skinLoop = true;
                                m_skinAnimator->play(&m_skinAnimations[0], m_skinLoop);
                            }
                        }
                    }
                    if (!m_skinAnimations.empty())
                    {
                        ImGui::Text("Animation");
                        ImGui::SliderInt("Index", &m_skinAnimIndex, 0, (int)m_skinAnimations.size()-1);
                        ImGui::SameLine();
                        if (ImGui::Button("Play")) { m_skinPlaying = true; m_skinAnimator->play(&m_skinAnimations[m_skinAnimIndex], m_skinLoop); }
                        ImGui::SameLine();
                        if (ImGui::Button("Stop")) { m_skinPlaying = false; }
                        ImGui::Checkbox("Loop", &m_skinLoop);
                        ImGui::SliderFloat("Speed", &m_skinSpeed, 0.1f, 3.0f);
                    }
                }
                ImGui::End();
            });

            // Tools
            m_ui->registerPanel([this]()
            {
                if (ImGui::Begin("Tools"))
                {
                    ImGui::Text("Shader Reloader");
                    ImGui::InputText("VS Path", m_vsPath, sizeof(m_vsPath));
                    ImGui::InputText("FS Path", m_fsPath, sizeof(m_fsPath));
                    if (ImGui::Button("Reload Shader") && m_vsPath[0] != '\0' && m_fsPath[0] != '\0')
                    {
                        auto loadFile = [](const char* path) -> std::string {
                            FILE* f = nullptr; fopen_s(&f, path, "rb"); if (!f) return {}; fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET); std::string s; s.resize(sz); fread(s.data(), 1, sz, f); fclose(f); return s;
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
                }
                ImGui::End();
            });
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
                // Auto-bind material assets from paths
                if (m_resources && m_scene)
                {
                    for (auto& e : m_scene->entities())
                    {
                        if (!e.materialPath.empty())
                        {
                            if (auto* mat = m_resources->getMaterialFromFile(e.materialPath))
                            {
                                e.material = mat;
                                e.usePBR = true;
                                if (mat->albedoTex) { e.albedoTex = mat->albedoTex; e.useTexture = true; }
                                if (mat->metallicTex) e.metallicTex = mat->metallicTex;
                                if (mat->roughnessTex) e.roughnessTex = mat->roughnessTex;
                                if (mat->aoTex) e.aoTex = mat->aoTex;
                                if (mat->normalTex) e.normalTex = mat->normalTex;
                                e.albedo[0]=mat->albedo[0]; e.albedo[1]=mat->albedo[1]; e.albedo[2]=mat->albedo[2];
                                e.metallic = mat->metallic; e.roughness = mat->roughness; e.ao = mat->ao;
                            }
                        }
                    }
                }
            }

            m_ui->beginFrame();
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
                ImGui::Separator();
                ImGui::Text("Post-process");
                ImGui::SliderFloat("Exposure", &m_exposure, 0.1f, 5.0f);
                ImGui::SliderFloat("Gamma", &m_gamma, 1.0f, 2.6f);
                ImGui::Checkbox("FXAA", &m_fxaa);
                ImGui::Checkbox("Frustum Culling", &m_frustumCulling);
                ImGui::Checkbox("Instancing (same Mesh)", &m_useInstancing);
                ImGui::Checkbox("Draw Colliders", &m_drawColliders);
                ImGui::Separator();
                ImGui::Text("Bloom");
                ImGui::Checkbox("Bloom Enabled", &m_bloomEnabled);
                ImGui::SliderFloat("Threshold", &m_bloomThreshold, 0.0f, 5.0f);
                ImGui::SliderFloat("Intensity", &m_bloomIntensity, 0.0f, 2.0f);
                ImGui::SliderInt("Iterations", &m_bloomIterations, 1, 10);
                ImGui::Separator();
                ImGui::Text("SSAO");
                ImGui::Checkbox("SSAO Enabled", &m_ssaoEnabled);
                ImGui::SliderFloat("Radius", &m_ssaoRadius, 0.05f, 2.0f);
                ImGui::SliderFloat("Bias", &m_ssaoBias, 0.0f, 0.1f, "%.3f");
                ImGui::SliderFloat("Power", &m_ssaoPower, 0.1f, 3.0f);
                ImGui::Separator();
                ImGui::Text("TAA");
                ImGui::Checkbox("TAA Enabled", &m_taaEnabled);
                ImGui::SliderFloat("TAA Alpha", &m_taaAlpha, 0.02f, 0.3f);
                ImGui::Separator();
                ImGui::Text("Particles");
                ImGui::Checkbox("Emit", &m_particlesEmit);
                ImGui::SliderFloat("Rate (p/s)", &m_particlesRate, 0.0f, 500.0f);
                ImGui::SliderFloat("Lifetime", &m_particlesLifetime, 0.1f, 5.0f);
                ImGui::SliderFloat("Size", &m_particlesSize, 1.0f, 20.0f);
                ImGui::SliderFloat("Gravity Y", &m_particlesGravityY, -20.0f, 20.0f);
                ImGui::ColorEdit3("Color", m_particlesColor);
                ImGui::Checkbox("Additive", &m_particlesAdditive);
                ImGui::Separator();
                ImGui::Checkbox("Enable Picking (LMB)", &m_enablePicking);
                ImGui::Checkbox("Push On Pick", &m_pushOnPick);
                ImGui::Checkbox("Auto Create Rigid On Push", &m_autoCreateRigidOnPush);
                ImGui::SliderFloat("Push Strength", &m_pushStrength, 0.0f, 200.0f);
                ImGui::Separator();
                ImGui::Text("Shadows");
                ImGui::Checkbox("Enable Shadows", &m_shadowsEnabled);
                ImGui::SliderFloat("Bias", &m_shadowBias, 0.0001f, 0.01f, "%.5f");
                ImGui::Checkbox("Enable CSM", &m_csmEnabled);
                ImGui::SliderInt("Cascade Count", &m_cascadeCount, 1, 4);
                ImGui::SliderInt("CSM Size", &m_csmSize, 256, 4096);
                ImGui::DragFloat4("Cascade Ends", m_cascadeEnds, 0.5f, 0.1f, 500.0f);
                ImGui::Checkbox("PCF", &m_usePCF); ImGui::SameLine(); ImGui::Checkbox("PCSS", &m_usePCSS);
                ImGui::SliderInt("PCF Kernel", &m_pcfKernel, 1, 4);
                ImGui::SliderFloat("Light Radius", &m_lightRadius, 0.0f, 2.0f);
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
                ImGui::Text("IBL");
                ImGui::Checkbox("Use IBL", &m_useIBL);
                ImGui::InputText("HDR Path", m_hdrPath, sizeof(m_hdrPath));
                if (ImGui::Button("Load HDR"))
                {
                    if (!m_ibl) m_ibl = std::make_unique<IBL>();
                    if (m_hdrPath[0]) m_useIBL = m_ibl->createFromHDR(m_hdrPath);
                }
                ImGui::Separator();
                ImGui::Text("Camera");
                ImGui::Checkbox("Orbit Mode (RMB drag + scroll)", &m_orbitMode);
                ImGui::SliderFloat("Speed", &m_cameraSpeed, 0.1f, 20.0f);
                ImGui::SliderFloat("Mouse Sensitivity", &m_mouseSensitivity, 0.01f, 1.0f);
                ImGui::Separator();
                ImGui::Text("Terrain");
                ImGui::InputText("Heightmap", m_heightPath, sizeof(m_heightPath));
                ImGui::InputText("SplatCtrl (RGBA)", m_splatCtrlPath, sizeof(m_splatCtrlPath));
                ImGui::InputText("Splat0", m_splat0Path, sizeof(m_splat0Path));
                ImGui::InputText("Splat1", m_splat1Path, sizeof(m_splat1Path));
                ImGui::InputText("Splat2", m_splat2Path, sizeof(m_splat2Path));
                ImGui::InputText("Splat3", m_splat3Path, sizeof(m_splat3Path));
                ImGui::InputText("NormalMap", m_normalPath, sizeof(m_normalPath));
                if (ImGui::Button("Load Terrain Textures"))
                {
                    if (m_terrain)
                    {
                        if (m_heightPath[0]) m_terrain->setHeightmap(m_resources->getTextureFromFile(m_heightPath, false));
                        if (m_splatCtrlPath[0]) m_terrain->setSplatControl(m_resources->getTextureFromFile(m_splatCtrlPath, false));
                        if (m_splat0Path[0]) m_terrain->setSplatTexture(0, m_resources->getTextureFromFile(m_splat0Path, false));
                        if (m_splat1Path[0]) m_terrain->setSplatTexture(1, m_resources->getTextureFromFile(m_splat1Path, false));
                        if (m_splat2Path[0]) m_terrain->setSplatTexture(2, m_resources->getTextureFromFile(m_splat2Path, false));
                        if (m_splat3Path[0]) m_terrain->setSplatTexture(3, m_resources->getTextureFromFile(m_splat3Path, false));
                        if (m_normalPath[0]) m_terrain->setNormalMap(m_resources->getTextureFromFile(m_normalPath, false));
                    }
                }
                ImGui::SliderInt("LOD", &m_terrainLOD, 0, 2);
                ImGui::SliderFloat("Height Scale", &m_terrainHeightScale, 1.0f, 200.0f);
                ImGui::SliderFloat("Splat Tiling", &m_terrainSplatTiling, 1.0f, 64.0f);
                ImGui::SliderFloat("Cell World Size", &m_terrainCellWorld, 0.25f, 8.0f);
                if (m_terrain)
                {
                    m_terrain->setLOD(m_terrainLOD);
                    m_terrain->setParams(m_terrainHeightScale, m_terrainSplatTiling, m_terrainCellWorld);
                }
                ImGui::Separator();
                ImGui::Text("Scripting (Lua)");
                ImGui::Checkbox("Hot Reload", &m_luaHotReload);
                if (m_lua) m_lua->setHotReloadEnabled(m_luaHotReload);
                if (ImGui::Button("Load Script (Selected Entity)"))
                {
                    int sel = m_scene->selectedIndex();
                    if (sel >= 0)
                    {
                        auto& e = m_scene->entities()[sel];
                        if (!e.scriptPath.empty() && m_lua)
                        {
                            m_lua->loadScript(e.scriptPath);
                            m_lua->bindEntity(sel, &e);
                            e.scriptEnabled = true;
                        }
                    }
                }
                ImGui::Separator();
                ImGui::Text("Audio");
                ImGui::InputText("Music (OGG/WAV)", m_musicPath, sizeof(m_musicPath));
                ImGui::SameLine();
                if (ImGui::Button("Play Music"))
                {
                    if (m_audio && m_musicPath[0])
                    {
                        auto buf = m_audio->loadSound(m_musicPath);
                        if (buf) m_musicSrc = m_audio->play(buf, 0.6f, true);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Stop Music"))
                {
                    if (m_audio && m_musicSrc) { m_audio->stop(m_musicSrc); m_musicSrc = 0; }
                }
                ImGui::InputText("SFX (OGG/WAV)", m_sfxPath, sizeof(m_sfxPath));
                if (ImGui::Button("Play SFX"))
                {
                    if (m_audio && m_sfxPath[0])
                    {
                        auto buf = m_audio->loadSound(m_sfxPath);
                        if (buf) m_audio->play3D(buf, 0.0f, 0.0f, 0.0f, 1.0f, false);
                    }
                }
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
                ImGui::Separator();
                ImGui::Text("Model Import (OBJ/FBX/GLTF)");
                ImGui::InputText("Model Path", m_modelPath, sizeof(m_modelPath));
                if (ImGui::Button("Import Model") && m_modelPath[0] != '\0')
                {
                    std::vector<ImportedMesh> ims;
                    if (AssimpLoader::loadModel(m_resources.get(), m_modelPath, ims, true))
                    {
                        for (auto& im : ims)
                        {
                            // Own the mesh memory inside Application to keep alive
                            std::unique_ptr<Mesh> owned(im.mesh);
                            int e = m_scene->addEntity(im.name.empty()?"Imported":im.name, owned.get(), m_shader.get(), im.diffuse);
                            m_scene->entities()[e].useTexture = (im.diffuse != nullptr);
                            // PBR guesses from Assimp
                            if (im.metal || im.rough || im.ao || im.normal)
                            {
                                auto& ent = m_scene->entities()[e];
                                ent.usePBR = true;
                                if (im.metal) ent.metallicTex = im.metal;
                                if (im.rough) ent.roughnessTex = im.rough;
                                if (im.ao) ent.aoTex = im.ao;
                                if (im.normal) ent.normalTex = im.normal;
                            }
                            m_importedMeshes.push_back(std::move(owned));
                        }
                    }
                }
                ImGui::Separator();
                ImGui::Text("Skinned Import (GLTF/FBX)");
                ImGui::InputText("Skinned Path", m_skinPath, sizeof(m_skinPath));
                if (ImGui::Button("Import Skinned") && m_skinPath[0] != '\0')
                {
                    ImportedSkinned isk;
                    if (AssimpLoader::loadSkinned(m_resources.get(), m_skinPath, isk, true))
                    {
                        m_skinMesh.reset(isk.mesh);
                        m_skinSkeleton.reset(isk.skeleton);
                        m_skinDiffuse = isk.diffuse;
                        m_skinAnimations = isk.animations ? *isk.animations : std::vector<Animation>();
                        delete isk.animations; // copied
                        if (!m_skinAnimator) m_skinAnimator = std::make_unique<Animator>();
                        if (!m_skinAnimations.empty())
                        {
                            m_skinAnimIndex = 0; m_skinPlaying = true; m_skinLoop = true;
                            m_skinAnimator->play(&m_skinAnimations[0], m_skinLoop);
                        }
                    }
                }
                if (!m_skinAnimations.empty())
                {
                    ImGui::Text("Animation");
                    ImGui::SliderInt("Index", &m_skinAnimIndex, 0, (int)m_skinAnimations.size()-1);
                    ImGui::SameLine();
                    if (ImGui::Button("Play")) { m_skinPlaying = true; m_skinAnimator->play(&m_skinAnimations[m_skinAnimIndex], m_skinLoop); }
                    ImGui::SameLine();
                    if (ImGui::Button("Stop")) { m_skinPlaying = false; }
                    ImGui::Checkbox("Loop", &m_skinLoop);
                    ImGui::SliderFloat("Speed", &m_skinSpeed, 0.1f, 3.0f);
                }

                // Hierarchy
                if (ImGui::Begin("Hierarchy (Legacy)"))
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
                            if (act) m_physBindings.push_back({ act, e });
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
                ImGui::End();
                // Save/Load ->
                if (m_ui) m_ui->drawPanels();
                // Save/Load
                static char scenePath[260] = "scene.json";
                ImGui::InputText("Scene Path", scenePath, sizeof(scenePath));
                if (ImGui::Button("Save (Ctrl+S)")) SceneSerializer::save(*m_scene, scenePath);
                ImGui::SameLine();
                if (ImGui::Button("Load (Ctrl+O)")) SceneSerializer::load(*m_scene, scenePath);
                ImGui::End();

                // Inspector
                if (ImGui::Begin("Inspector (Legacy)"))
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
                                createPhysicsForEntity(sel);
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
                        // Material asset load/save
                        static char matPath[260] = "";
                        ImGui::InputText("Material (.mat.json)", matPath, sizeof(matPath));
                        if (ImGui::Button("Load Material"))
                        {
                            if (matPath[0])
                            {
                                auto* mat = m_resources->getMaterialFromFile(matPath);
                                if (mat)
                                {
                                    ent.material = mat;
                                    ent.materialPath = mat->sourcePath.empty()? std::string(matPath) : mat->sourcePath;
                                    ent.usePBR = true;
                                    // Apply textures
                                    if (mat->albedoTex) { ent.albedoTex = mat->albedoTex; ent.useTexture = true; }
                                    if (mat->metallicTex) ent.metallicTex = mat->metallicTex;
                                    if (mat->roughnessTex) ent.roughnessTex = mat->roughnessTex;
                                    if (mat->aoTex) ent.aoTex = mat->aoTex;
                                    if (mat->normalTex) ent.normalTex = mat->normalTex;
                                    ent.albedo[0]=mat->albedo[0]; ent.albedo[1]=mat->albedo[1]; ent.albedo[2]=mat->albedo[2];
                                    ent.metallic = mat->metallic; ent.roughness = mat->roughness; ent.ao = mat->ao;
                                }
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Save Material"))
                        {
                            if (matPath[0])
                            {
                                MaterialAsset tmp;
                                tmp.albedo[0]=ent.albedo[0]; tmp.albedo[1]=ent.albedo[1]; tmp.albedo[2]=ent.albedo[2];
                                tmp.metallic=ent.metallic; tmp.roughness=ent.roughness; tmp.ao=ent.ao;
                                // try to resolve paths from ResourceManager cache if available
                                if (auto p = m_resources->getPathForTexture(ent.albedoTex)) tmp.albedoTexPath = *p;
                                if (auto p = m_resources->getPathForTexture(ent.metallicTex)) tmp.metallicTexPath = *p;
                                if (auto p = m_resources->getPathForTexture(ent.roughnessTex)) tmp.roughnessTexPath = *p;
                                if (auto p = m_resources->getPathForTexture(ent.aoTex)) tmp.aoTexPath = *p;
                                if (auto p = m_resources->getPathForTexture(ent.normalTex)) tmp.normalTexPath = *p;
                                tmp.saveToFile(matPath);
                                ent.materialPath = matPath;
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
                        ImGui::Separator();
                        ImGui::Text("Script");
                        static char scriptBuf[260] = "";
                        if (!ent.scriptPath.empty()) strncpy_s(scriptBuf, ent.scriptPath.c_str(), sizeof(scriptBuf)-1);
                        if (ImGui::InputText("Lua Path", scriptBuf, sizeof(scriptBuf))) { ent.scriptPath = scriptBuf; }
                        ImGui::Checkbox("Enabled", &ent.scriptEnabled);
                        if (ImGui::Button("Unload Script")) { if (m_lua && !ent.scriptPath.empty()) m_lua->unloadScript(ent.scriptPath); ent.scriptEnabled = false; }
                        ImGui::Separator();
                        ImGui::Text("PBR");
                        ImGui::Checkbox("Use PBR", &ent.usePBR);
                        ImGui::SliderFloat("Metallic", &ent.metallic, 0.0f, 1.0f);
                        ImGui::SliderFloat("Roughness", &ent.roughness, 0.04f, 1.0f);
                        ImGui::SliderFloat("AO", &ent.ao, 0.0f, 1.0f);
                        static char texM[260] = ""; static char texR[260] = ""; static char texA[260] = "";
                        if (ImGui::InputText("Metallic Tex", texM, sizeof(texM))) {}
                        ImGui::SameLine(); if (ImGui::Button("Load M")) { if (texM[0]) ent.metallicTex = m_resources->getTextureFromFile(texM, false); }
                        if (ImGui::InputText("Roughness Tex", texR, sizeof(texR))) {}
                        ImGui::SameLine(); if (ImGui::Button("Load R")) { if (texR[0]) ent.roughnessTex = m_resources->getTextureFromFile(texR, false); }
                        if (ImGui::InputText("AO Tex", texA, sizeof(texA))) {}
                        ImGui::SameLine(); if (ImGui::Button("Load AO")) { if (texA[0]) ent.aoTex = m_resources->getTextureFromFile(texA, false); }
                        static char texN[260] = "";
                        if (ImGui::InputText("Normal Tex", texN, sizeof(texN))) {}
                        ImGui::SameLine(); if (ImGui::Button("Load N")) { if (texN[0]) ent.normalTex = m_resources->getTextureFromFile(texN, false); }
                    }
                }
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
            // Bind HDR FBO for scene rendering
            if (m_post)
                m_post->begin(display_w, display_h, m_clearColor.x * m_clearColor.w, m_clearColor.y * m_clearColor.w, m_clearColor.z * m_clearColor.w, m_clearColor.w);
            else
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

            // Lua update
            if (m_lua) m_lua->onUpdate(dt);

            // Shadow pass (directional light with orthographic proj)
            glm::mat4 lightView = glm::lookAt(
                glm::vec3(m_lightPos[0], m_lightPos[1], m_lightPos[2]),
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(0,1,0));
            glm::mat4 lightProj = glm::ortho(-m_shadowOrthoSize, m_shadowOrthoSize, -m_shadowOrthoSize, m_shadowOrthoSize, m_shadowNear, m_shadowFar);
            glm::mat4 lightVP = lightProj * lightView;
            if (m_shadowsEnabled)
            {
                if (!m_csmEnabled)
                {
                    m_shadowMap->begin();
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
                else
                {
                    int cascades = m_cascadeCount;
                    if (!m_csm || m_csm->size() != m_csmSize || m_csm->cascades() != cascades)
                    {
                        if (!m_csm) m_csm = std::make_unique<CascadedShadowMap>();
                        else m_csm->destroy();
                        m_csm->create(m_csmSize, cascades);
                    }
                    float prevEnd = m_shadowNear;
                    for (int c = 0; c < cascades; ++c)
                    {
                        float endZ = m_cascadeEnds[c];
                        glm::mat4 proj = glm::ortho(-m_shadowOrthoSize, m_shadowOrthoSize, -m_shadowOrthoSize, m_shadowOrthoSize, prevEnd, endZ);
                        glm::mat4 vp = proj * lightView;
                        memcpy(m_cascadeMatrices[c], &vp[0][0], sizeof(float)*16);
                        m_csm->beginCascade(c);
                        for (const auto& e : m_scene->getEntities())
                        {
                            glm::mat4 model = e.transform.modelMatrix();
                            m_depthShader->bind();
                            m_depthShader->setMat4("u_LightVP", &vp[0][0]);
                            m_depthShader->setMat4("u_Model", &model[0][0]);
                            e.mesh->draw();
                        }
                        m_depthShader->unbind();
                        prevEnd = endZ;
                    }
                    m_csm->end(display_w, display_h);
                }
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

            // Re-bind HDR FBO after shadow passes (they restore default FBO)
            if (m_post)
                m_post->bind(display_w, display_h);

            // Skinned update & draw
            if (m_skinMesh && m_skinSkeleton && m_skinAnimator)
            {
                if (m_skinPlaying)
                {
                    // temporarily scale dt by speed by advancing animator time outside; update uses dt directly
                    float dtLocal = dt * m_skinSpeed;
                    m_skinAnimator->update(*m_skinSkeleton, dtLocal);
                }
                // upload bones
                const int maxBones = 128;
                int count = (int)std::min<size_t>(m_skinSkeleton->posePalette.size(), maxBones);
                m_skinShader->bind();
                glm::mat4 model = glm::mat4(1.0f);
                glm::mat4 vp = m_camera->projection() * m_camera->view();
                glm::mat3 nrm = glm::mat3(glm::transpose(glm::inverse(model)));
                m_skinShader->setMat4("u_Model", &model[0][0]);
                m_skinShader->setMat4("u_VP", &vp[0][0]);
                m_skinShader->setMat3("u_NormalMatrix", &nrm[0][0]);
                m_skinShader->setVec3("u_CameraPos", m_camera->position().x, m_camera->position().y, m_camera->position().z);
                m_skinShader->setVec3("u_LightPos", m_lightPos[0], m_lightPos[1], m_lightPos[2]);
                m_skinShader->setVec3("u_LightColor", m_lightColor[0], m_lightColor[1], m_lightColor[2]);
                m_skinShader->setVec3("u_Albedo", 1.0f, 1.0f, 1.0f);
                m_skinShader->setFloat("u_Shininess", 64.0f);
                if (m_skinDiffuse)
                {
                    m_skinShader->setInt("u_UseTexture", 1);
                    m_skinShader->setInt("u_AlbedoTex", 0);
                    m_skinDiffuse->bind(0);
                }
                else
                {
                    m_skinShader->setInt("u_UseTexture", 0);
                }
                if (count > 0)
                    m_skinShader->setMat4Array("u_Bones", &m_skinSkeleton->posePalette[0][0][0], count);
                m_skinMesh->draw();
                m_skinShader->unbind();
            }

            // Frustum culling visibility compute
            if (m_frustumCulling)
            {
                glm::mat4 vp = m_camera->projection() * m_camera->view();
                computeCameraFrustum(&vp[0][0]);
                m_frustumVisible.assign(m_scene->getEntities().size(), 1);
                for (size_t i=0;i<m_scene->getEntities().size();++i)
                {
                    const auto& e = m_scene->getEntities()[i];
                    float center[3] = { e.transform.position.x, e.transform.position.y, e.transform.position.z };
                    float radius = 1.0f * std::max({e.transform.scale.x, e.transform.scale.y, e.transform.scale.z});
                    if (!sphereInFrustum(center, radius)) m_frustumVisible[i] = 0;
                }
            }

            // Render scene
            for (size_t ei=0; ei<m_scene->getEntities().size(); ++ei)
            {
                const auto& e = m_scene->getEntities()[ei];
                if (m_frustumCulling && ei < m_frustumVisible.size() && m_frustumVisible[ei]==0) continue;
                glm::mat4 model = e.transform.modelMatrix();
                glm::mat4 mvp = m_camera->projection() * m_camera->view() * model;
                glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(model)));

                if (e.usePBR && m_pbrShader)
                {
                    m_pbrShader->bind();
                    m_pbrShader->setMat4("u_MVP", &mvp[0][0]);
                    m_pbrShader->setMat4("u_Model", &model[0][0]);
                    m_pbrShader->setMat3("u_NormalMatrix", &normalMat[0][0]);
                    m_pbrShader->setVec3("u_Cam", m_camera->position().x, m_camera->position().y, m_camera->position().z);
                    m_pbrShader->setVec3("u_LightPos", m_lightPos[0], m_lightPos[1], m_lightPos[2]);
                    m_pbrShader->setVec3("u_Albedo", e.albedo[0], e.albedo[1], e.albedo[2]);
                    m_pbrShader->setFloat("u_Metallic", e.metallic);
                    m_pbrShader->setFloat("u_Roughness", e.roughness);
                    m_pbrShader->setFloat("u_AO", e.ao);
                    int useAlbedoTex = (e.useTexture && e.albedoTex) ? 1 : 0;
                    m_pbrShader->setInt("u_UseAlbedoTex", useAlbedoTex);
                    if (useAlbedoTex) { m_pbrShader->setInt("u_AlbedoTex", 0); e.albedoTex->bind(0); }
                    int useM = (e.metallicTex!=nullptr)?1:0; m_pbrShader->setInt("u_UseMetalTex", useM); if (useM){ m_pbrShader->setInt("u_MetalTex",1); e.metallicTex->bind(1);} 
                    int useR = (e.roughnessTex!=nullptr)?1:0; m_pbrShader->setInt("u_UseRoughTex", useR); if (useR){ m_pbrShader->setInt("u_RoughTex",2); e.roughnessTex->bind(2);} 
                    int useA = (e.aoTex!=nullptr)?1:0; m_pbrShader->setInt("u_UseAOTex", useA); if (useA){ m_pbrShader->setInt("u_AOTex",3); e.aoTex->bind(3);} 
                    int useN = (e.normalTex!=nullptr)?1:0; m_pbrShader->setInt("u_UseNormalMap", useN); if (useN){ m_pbrShader->setInt("u_NormalTex",4); e.normalTex->bind(4);} 
                    if (m_useIBL && m_ibl && m_ibl->valid())
                    {
                        m_pbrShader->setInt("u_UseIBL", 1);
                        m_pbrShader->setInt("u_IrradianceMap", 5); glActiveTexture(GL_TEXTURE0+5); glBindTexture(GL_TEXTURE_CUBE_MAP, m_ibl->irradianceMap());
                        m_pbrShader->setInt("u_PrefilterMap", 6); glActiveTexture(GL_TEXTURE0+6); glBindTexture(GL_TEXTURE_CUBE_MAP, m_ibl->prefilterMap());
                        m_pbrShader->setInt("u_BRDFLUT", 7); glActiveTexture(GL_TEXTURE0+7); glBindTexture(GL_TEXTURE_2D, m_ibl->brdfLUT());
                    }
                    else m_pbrShader->setInt("u_UseIBL", 0);
                    // Shadows PCF/PCSS + CSM
                    m_pbrShader->setInt("u_ShadowsEnabled", m_shadowsEnabled ? 1 : 0);
                    if (!m_csmEnabled)
                    {
                        m_pbrShader->setInt("u_UseCSM", 0);
                        m_pbrShader->setMat4("u_LightVP", &lightVP[0][0]);
                        m_pbrShader->setFloat("u_ShadowBias", m_shadowBias);
                        m_shadowMap->bindDepthTexture(8);
                        m_pbrShader->setInt("u_ShadowMap", 8);
                        m_pbrShader->setInt("u_PCFKernel", m_usePCF ? m_pcfKernel : 0);
                        m_pbrShader->setFloat("u_ShadowMapSize", (float)m_shadowMapSize);
                        m_pbrShader->setInt("u_UsePCSS", m_usePCSS ? 1 : 0);
                        m_pbrShader->setFloat("u_LightRadius", m_lightRadius);
                    }
                    else
                    {
                        m_pbrShader->setInt("u_UseCSM", 1);
                        int cascades = m_cascadeCount;
                        for (int c = 0; c < cascades; ++c)
                        {
                            char name[32]; sprintf_s(name, "u_CascadeVP[%d]", c);
                            m_pbrShader->setMat4(name, m_cascadeMatrices[c]);
                            m_csm->bindCascade(c, 8 + c);
                            char smp[32]; sprintf_s(smp, "u_CascadeMap[%d]", c);
                            m_pbrShader->setInt(smp, 8 + c);
                        }
                        m_pbrShader->setInt("u_CascadeCount", cascades);
                        m_pbrShader->setFloat("u_ShadowBias", m_shadowBias);
                        m_pbrShader->setInt("u_PCFKernel", m_usePCF ? m_pcfKernel : 0);
                        m_pbrShader->setFloat("u_ShadowMapSize", (float)m_csmSize);
                        m_pbrShader->setInt("u_UsePCSS", m_usePCSS ? 1 : 0);
                        m_pbrShader->setFloat("u_LightRadius", m_lightRadius);
                    }
                    e.mesh->draw();
                    m_pbrShader->unbind();
                    continue;
                }

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
                if (!m_csmEnabled)
                {
                    e.shader->setMat4("u_LightVP", &lightVP[0][0]);
                    e.shader->setFloat("u_ShadowBias", m_shadowBias);
                    m_shadowMap->bindDepthTexture(1);
                    e.shader->setInt("u_ShadowMap", 1);
                    e.shader->setInt("u_UseCSM", 0);
                    e.shader->setInt("u_PCFKernel", m_usePCF ? m_pcfKernel : 0);
                    e.shader->setFloat("u_ShadowMapSize", (float)m_shadowMapSize);
                    e.shader->setInt("u_UsePCSS", m_usePCSS ? 1 : 0);
                    e.shader->setFloat("u_LightRadius", m_lightRadius);
                }
                else
                {
                    int cascades = m_cascadeCount;
                    for (int c = 0; c < cascades; ++c)
                    {
                        char name[32]; sprintf_s(name, "u_CascadeVP[%d]", c);
                        e.shader->setMat4(name, m_cascadeMatrices[c]);
                        m_csm->bindCascade(c, 1 + c);
                        char smp[32]; sprintf_s(smp, "u_CascadeMap[%d]", c);
                        e.shader->setInt(smp, 1 + c);
                    }
                    e.shader->setInt("u_CascadeCount", cascades);
                    e.shader->setInt("u_UseCSM", 1);
                    e.shader->setFloat("u_ShadowBias", m_shadowBias);
                    e.shader->setInt("u_PCFKernel", m_usePCF ? m_pcfKernel : 0);
                    e.shader->setFloat("u_ShadowMapSize", (float)m_csmSize);
                    e.shader->setInt("u_UsePCSS", m_usePCSS ? 1 : 0);
                    e.shader->setFloat("u_LightRadius", m_lightRadius);
                }
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
                // point light
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

            // Terrain draw
            if (m_terrain)
            {
                m_terrain->draw(
                    m_camera->projection(),
                    m_camera->view(),
                    m_camera->position(),
                    glm::vec3(m_lightPos[0], m_lightPos[1], m_lightPos[2]),
                    glm::vec3(m_lightColor[0], m_lightColor[1], m_lightColor[2])
                );
            }

            // Update & draw particles (after opaque)
            if (m_particles)
            {
                m_particles->update(dt,
                    m_particlesEmit,
                    glm::vec3(0.0f, 1.0f, 0.0f),
                    m_particlesRate,
                    m_particlesLifetime,
                    m_particlesSize,
                    glm::vec3(m_particlesColor[0], m_particlesColor[1], m_particlesColor[2]),
                    m_particlesGravityY,
                    m_particlesAdditive);
                m_particles->draw(&m_camera->projection()[0][0], &m_camera->view()[0][0]);
            }

            // Update audio listener from camera
            if (m_audio)
            {
                glm::vec3 camPos = m_camera->position();
                // approximate forward/up from view matrix
                glm::mat4 V = m_camera->view();
                glm::vec3 f(-V[0][2], -V[1][2], -V[2][2]);
                glm::vec3 u(V[0][1], V[1][1], V[2][1]);
                m_audio->setListener(camPos.x, camPos.y, camPos.z, f.x, f.y, f.z, u.x, u.y, u.z);
            }

            // Finalize ImGui and render draw data
            m_ui->endFrame();
            // Post-process to screen, then draw ImGui on top
            if (m_post)
                m_post->drawToScreen(display_w, display_h, m_exposure, m_gamma, m_fxaa,
                    m_bloomEnabled, m_bloomThreshold, m_bloomIntensity, m_bloomIterations,
                    m_ssaoEnabled, m_ssaoRadius, m_ssaoBias, m_ssaoPower,
                    m_taaEnabled, m_taaAlpha);
            m_ui->renderDrawData();

            m_window->swapBuffers();
        }

        shutdown();
        std::cout << "[App] Shutdown completed" << std::endl;
        return 0;
    }

    void Application::shutdown()
    {
        if (m_ui) { m_ui->shutdown(); m_ui.reset(); }
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

    bool Application::initializeImGui() { return true; }
    void Application::shutdownImGui() {}
}


