#include "ui/UIManager.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

namespace engine
{
    UIManager::UIManager() = default;
    UIManager::~UIManager() = default;

    bool UIManager::initialize(GLFWwindow* window)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, false);
        ImGui_ImplOpenGL3_Init("#version 330");
        return true;
    }

    void UIManager::beginFrame()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void UIManager::registerPanel(const std::function<void()>& panel)
    {
        m_panels.push_back(panel);
    }

    void UIManager::drawPanels()
    {
        for (auto& p : m_panels) p();
    }

    void UIManager::endFrame()
    {
        ImGui::Render();
    }

    void UIManager::renderDrawData()
    {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void UIManager::shutdown()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        if (ImGui::GetCurrentContext())
            ImGui::DestroyContext();
    }
}


