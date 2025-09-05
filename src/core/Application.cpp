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

        // Create shader (Phong + texture toggle)
        m_shader = std::make_unique<Shader>();
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
            void main()
            {
                vec3 N = normalize(vNormal);
                vec3 L = normalize(u_LightPos - vWorldPos);
                vec3 V = normalize(u_CameraPos - vWorldPos);
                vec3 H = normalize(L + V);
                float diff = max(dot(N, L), 0.0);
                float spec = pow(max(dot(N, H), 0.0), u_Shininess);
                vec3 baseColor = u_UseTexture ? texture(u_AlbedoTex, vUV).rgb : u_Albedo;
                vec3 color = baseColor * (0.1 + diff) + u_LightColor * spec;
                FragColor = vec4(color, 1.0);
            }
        )GLSL";
        if (!m_shader->compileFromSource(vs, fs))
        {
            std::cerr << "[App] Shader compile failed" << std::endl;
            return false;
        }

        // Cube mesh
        m_cube = std::make_unique<Mesh>(Mesh::createCube());

        // Camera
        m_camera = std::make_unique<Camera>();
        float aspect = fbw > 0 && fbh > 0 ? (float)fbw / (float)fbh : 16.0f/9.0f;
        m_camera->setPerspective(45.0f * 3.14159265f / 180.0f, aspect, 0.1f, 100.0f);
        m_camera->setView({2.0f, 2.0f, 2.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});

        // Time
        m_time = std::make_unique<Time>();
        m_time->reset();

        // Texture (checkerboard)
        m_texture = std::make_unique<Texture2D>();
        m_texture->createCheckerboard(256, 256, 32);

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

            // Draw cube with Phong lighting
            static float angle = 0.0f;
            angle += dt;
            glm::mat4 model(1.0f);
            model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 mvp = m_camera->projection() * m_camera->view() * model;
            glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(model)));

            m_shader->bind();
            m_shader->setMat4("u_MVP", &mvp[0][0]);
            m_shader->setMat4("u_Model", &model[0][0]);
            m_shader->setMat3("u_NormalMatrix", &normalMat[0][0]);
            m_shader->setVec3("u_CameraPos", m_camera->position().x, m_camera->position().y, m_camera->position().z);
            m_shader->setVec3("u_LightPos", m_lightPos[0], m_lightPos[1], m_lightPos[2]);
            m_shader->setVec3("u_LightColor", m_lightColor[0], m_lightColor[1], m_lightColor[2]);
            m_shader->setVec3("u_Albedo", m_albedo[0], m_albedo[1], m_albedo[2]);
            m_shader->setFloat("u_Shininess", m_shininess);
            // texture bindings
            if (m_useTexture)
            {
                m_shader->setFloat("u_UseTexture", 1.0f);
                m_texture->bind(0);
                // sampler assumed at location 0 by default
            }
            else
            {
                m_shader->setFloat("u_UseTexture", 0.0f);
            }
            m_cube->draw();
            m_shader->unbind();

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


