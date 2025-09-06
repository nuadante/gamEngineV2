#pragma once

#include <string>

namespace engine
{
    class ResourceManager;
    struct ECS;

    // JSON tabanli ECS sahne kaydet/yukle
    class ECSSerializer
    {
    public:
        static bool save(ECS& ecs, const std::string& path);
        static bool load(ECS& ecs, const std::string& path, ResourceManager* resources);
    };
}



