cmake_minimum_required(VERSION 3.15)
project(ImGuiProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Conan integration
find_package(imgui REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glad REQUIRED)
find_package(glm REQUIRED)
find_package(stb REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(physx REQUIRED)
find_package(assimp REQUIRED)
find_package(lua REQUIRED)
find_package(OpenAL REQUIRED)

# Create executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/core/Application.cpp
    src/physics/Physics.cpp
    src/platform/Window.cpp
    src/input/InputManager.cpp
    src/render/Shader.cpp
    src/render/Mesh.cpp
    src/render/Renderer.cpp
    src/render/Camera.cpp
    src/render/Texture2D.cpp
    src/render/ShadowMap.cpp
    src/render/PointShadowMap.cpp
    src/render/PostProcess.cpp
    src/render/Skybox.cpp
    src/render/AssimpLoader.cpp
    src/render/SkinnedMesh.cpp
    src/render/Animator.cpp
    src/render/Terrain.cpp
    src/render/ParticleSystem.cpp
    src/render/Material.cpp
    src/render/IBL.cpp
    src/core/ResourceManager.cpp
    src/scene/SceneSerializer.cpp
    src/scripting/LuaEngine.cpp
    src/audio/AudioEngine.cpp
    src/scene/Scene.h
    src/scene/Transform.h
)

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    imgui::imgui
    glfw
    glad::glad
    glm::glm
    stb::stb
    nlohmann_json::nlohmann_json
    physx::physx
    PhysX::PhysXExtensions
    assimp::assimp
    lua::lua
    OpenAL::OpenAL
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Add ImGui backend sources
file(GLOB IMGUI_BACKEND_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/*.cpp
)

# Add ImGui backend sources to target
target_sources(${PROJECT_NAME} PRIVATE ${IMGUI_BACKEND_SOURCES})

# Include ImGui backends directory
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
)

# Platform specific settings
if(WIN32)
    target_link_libraries(${PROJECT_NAME} opengl32)
elseif(APPLE)
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
else()
    target_link_libraries(${PROJECT_NAME} GL)
endif()
