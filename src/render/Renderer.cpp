#include "render/Renderer.h"

#include <glad/glad.h>

namespace engine
{
    void Renderer::initialize()
    {
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_MULTISAMPLE);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        glEnable(GL_LINE_SMOOTH);
    }

    void Renderer::beginFrame(int width, int height, float r, float g, float b, float a)
    {
        glViewport(0, 0, width, height);
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void Renderer::endFrame()
    {
    }

    void Renderer::shutdown()
    {
    }

    void Renderer::setWireframe(bool enabled)
    {
        glPolygonMode(GL_FRONT_AND_BACK, enabled ? GL_LINE : GL_FILL);
    }
}


