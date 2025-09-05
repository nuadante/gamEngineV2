#pragma once

#include <string>

namespace engine
{
    class Scene;

    class SceneSerializer
    {
    public:
        static bool save(const Scene& scene, const std::string& path);
        static bool load(Scene& scene, const std::string& path);
    };
}


