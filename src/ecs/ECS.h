#pragma once

#include <string>
#include <glm/glm.hpp>
#include <entt/entt.hpp>

namespace engine
{
    struct TagC { std::string name; };

    struct TransformC
    {
        glm::vec3 position{0.0f};
        glm::vec3 rotationEuler{0.0f};
        glm::vec3 scale{1.0f};
    };

    class Mesh;
    struct MeshRendererC
    {
        Mesh* mesh{nullptr};
        // Material pointer is optional and reused from existing system for now
        struct MaterialAsset* material{nullptr};
        // Legacy texture support
        class Texture2D* albedoTex{nullptr};
        bool usePBR{false};
        std::string materialPath; // for serialization
    };

    struct DirectionalLightC
    {
        glm::vec3 color{1.0f,1.0f,1.0f};
        float intensity{1.0f};
        glm::vec3 direction{-0.5f,-1.0f,-0.3f};
    };

    struct PointLightC
    {
        glm::vec3 color{1.0f,1.0f,1.0f};
        float intensity{1.0f};
        float range{25.0f};
    };

    struct SpotLightC
    {
        glm::vec3 color{1.0f,1.0f,1.0f};
        float intensity{1.0f};
        glm::vec3 direction{0.0f,-1.0f,0.0f};
        float innerDegrees{15.0f};
        float outerDegrees{25.0f};
        float nearPlane{0.1f};
        float farPlane{40.0f};
    };

    struct ParticleEmitterC
    {
        bool emit{true};
        float rate{50.0f};
    };

    struct BoundsC { float radius{1.0f}; };

    struct ECS
    {
        entt::registry registry;

        entt::entity createEntity(const std::string& name)
        {
            entt::entity e = registry.create();
            registry.emplace<TagC>(e, name);
            registry.emplace<TransformC>(e);
            return e;
        }
    };
}


