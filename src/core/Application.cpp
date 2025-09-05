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
#include "render/Skybox.h"
#include "scene/Scene.h"
#include "scene/Transform.h"
#include "core/ResourceManager.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/Time.h"

namespace engine
{
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
            uniform vec3 u_Albedo;
            uniform float u_Shininess;
            uniform bool u_UseTexture;
            uniform sampler2D u_AlbedoTex;
            // shadows
            uniform bool u_ShadowsEnabled;
            uniform mat4 u_LightVP;
            uniform sampler2D u_ShadowMap;
            uniform float u_ShadowBias;
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

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

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
                ImGui::Text("Shadows");
                ImGui::Checkbox("Enable Shadows", &m_shadowsEnabled);
                ImGui::SliderFloat("Bias", &m_shadowBias, 0.0001f, 0.01f, "%.5f");
                ImGui::Separator();
                ImGui::Text("Skybox");
                ImGui::ColorEdit3("Top", m_skyTop);
                ImGui::ColorEdit3("Bottom", m_skyBottom);
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
                        if (sel >= 0) m_scene->removeEntity(sel);
                    }
                    auto& es = m_scene->entities();
                    for (int i = 0; i < (int)es.size(); ++i)
                    {
                        bool selected = (m_scene->selectedIndex() == i);
                        if (ImGui::Selectable(es[i].name.c_str(), selected))
                        {
                            m_scene->setSelectedIndex(i);
                        }
                    }
                }
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

            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(m_window->getNativeHandle(), &display_w, &display_h);
            Renderer::setWireframe(m_wireframe);
            glfwSwapInterval(m_vsync ? 1 : 0);
            Renderer::beginFrame(display_w, display_h, m_clearColor.x * m_clearColor.w, m_clearColor.y * m_clearColor.w, m_clearColor.z * m_clearColor.w, m_clearColor.w);

            // Camera controls
            float dt = m_time->tick();
            double mdx, mdy; m_input->getCursorDelta(mdx, mdy);
            const float mouseSensitivity = 0.1f;
            if (m_input->isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT))
            {
                m_camera->addYawPitch((float)(mdx * mouseSensitivity), (float)(-mdy * mouseSensitivity));
            }
            glm::vec3 move(0.0f);
            const float speed = 3.0f;
            if (m_input->isKeyPressed(GLFW_KEY_W)) move.z += speed * dt;
            if (m_input->isKeyPressed(GLFW_KEY_S)) move.z -= speed * dt;
            if (m_input->isKeyPressed(GLFW_KEY_A)) move.x -= speed * dt;
            if (m_input->isKeyPressed(GLFW_KEY_D)) move.x += speed * dt;
            if (m_input->isKeyPressed(GLFW_KEY_E)) move.y += speed * dt;
            if (m_input->isKeyPressed(GLFW_KEY_Q)) move.y -= speed * dt;
            if (move.x != 0 || move.y != 0 || move.z != 0) m_camera->moveLocal(move);

            // Update cube rotation
            static float angle = 0.0f;
            angle += dt;
            // animate each entity slightly differently
            for (size_t i = 0; i < m_scene->entities().size(); ++i)
            {
                m_scene->entities()[i].transform.rotationEuler.y = angle * (1.0f + 0.2f * (float)i);
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
                e.mesh->draw();
                e.shader->unbind();
            }

            // Draw skybox last
            m_skybox->draw(m_camera->projection(), m_camera->view(), {m_skyTop[0], m_skyTop[1], m_skyTop[2]}, {m_skyBottom[0], m_skyBottom[1], m_skyBottom[2]});

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


