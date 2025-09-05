cmake_minimum_required(VERSION 3.15)
project(ImGuiProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Conan integration
find_package(imgui REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glad REQUIRED)
find_package(glm REQUIRED)

# Create executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/core/Application.cpp
    src/platform/Window.cpp
    src/input/InputManager.cpp
    src/render/Shader.cpp
    src/render/Mesh.cpp
    src/render/Renderer.cpp
    src/render/Camera.cpp
)

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    imgui::imgui
    glfw
    glad::glad
    glm::glm
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
