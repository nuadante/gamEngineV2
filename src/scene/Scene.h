#pragma once

#include <vector>
#include <string>

#include "scene/Transform.h"

namespace engine
{
    class Mesh;
    class Shader;
    class Texture2D;

    struct Entity
    {
        std::string name;
        Transform transform;
        Mesh* mesh = nullptr;
        Shader* shader = nullptr;
        Texture2D* albedoTex = nullptr; // optional
        // per-entity material
        float albedo[3] = {1.0f, 0.7f, 0.2f};
        float shininess = 64.0f;
        bool useTexture = true;
        // physics
        bool hasRigidBody = false;
        bool isKinematic = false; // if dynamic and kinematic
        bool isStatic = false;    // static actor
        float mass = 1.0f;
        float friction = 0.5f;
        float restitution = 0.1f;
        // box collider half-extents
        float colliderHalf[3] = {0.5f, 0.5f, 0.5f};
        // scripting
        std::string scriptPath; // lua file path
        bool scriptEnabled = false;
        // runtime: not serialized (PhysX actor handle managed elsewhere)
    };

    class Scene
    {
    public:
        int addEntity(const std::string& name, Mesh* mesh, Shader* shader, Texture2D* tex)
        {
            Entity e{};
            e.name = name;
            e.mesh = mesh;
            e.shader = shader;
            e.albedoTex = tex;
            m_entities.push_back(e);
            return static_cast<int>(m_entities.size()) - 1;
        }

        std::vector<Entity>& entities() { return m_entities; }
        const std::vector<Entity>& getEntities() const { return m_entities; }

        int selectedIndex() const { return m_selected; }
        void setSelectedIndex(int idx) { m_selected = idx; }

        void removeEntity(int idx)
        {
            if (idx < 0 || idx >= (int)m_entities.size()) return;
            m_entities.erase(m_entities.begin() + idx);
            if (m_selected >= (int)m_entities.size()) m_selected = (int)m_entities.size() - 1;
        }

    private:
        std::vector<Entity> m_entities;
        int m_selected = -1;
    };
}


