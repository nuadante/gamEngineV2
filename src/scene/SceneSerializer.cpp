#include "scene/SceneSerializer.h"
#include "scene/Scene.h"
#include "core/ResourceManager.h"
#include "render/Material.h"

#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace engine
{
    static json entityToJson(const Entity& e)
    {
        json j;
        j["name"] = e.name;
        j["position"] = { e.transform.position.x, e.transform.position.y, e.transform.position.z };
        j["rotation"] = { e.transform.rotationEuler.x, e.transform.rotationEuler.y, e.transform.rotationEuler.z };
        j["scale"]    = { e.transform.scale.x,    e.transform.scale.y,    e.transform.scale.z };
        j["albedo"]   = { e.albedo[0], e.albedo[1], e.albedo[2] };
        j["shininess"] = e.shininess;
        j["useTexture"] = e.useTexture;
        // PBR/material
        j["usePBR"] = e.usePBR;
        j["metallic"] = e.metallic;
        j["roughness"] = e.roughness;
        j["ao"] = e.ao;
        if (!e.materialPath.empty()) j["materialPath"] = e.materialPath;
        // physics
        j["hasRigidBody"] = e.hasRigidBody;
        j["isKinematic"] = e.isKinematic;
        j["isStatic"] = e.isStatic;
        j["mass"] = e.mass;
        j["friction"] = e.friction;
        j["restitution"] = e.restitution;
        j["colliderHalf"] = { e.colliderHalf[0], e.colliderHalf[1], e.colliderHalf[2] };
        return j;
    }

    static void jsonToEntity(const json& j, Entity& e)
    {
        e.name = j.value("name", std::string("Entity"));
        auto p = j.value("position", std::vector<float>{0,0,0});
        auto r = j.value("rotation", std::vector<float>{0,0,0});
        auto s = j.value("scale",    std::vector<float>{1,1,1});
        auto a = j.value("albedo",   std::vector<float>{1,1,1});
        e.transform.position = { p[0], p[1], p[2] };
        e.transform.rotationEuler = { r[0], r[1], r[2] };
        e.transform.scale = { s[0], s[1], s[2] };
        e.albedo[0] = a[0]; e.albedo[1] = a[1]; e.albedo[2] = a[2];
        e.shininess = j.value("shininess", 64.0f);
        e.useTexture = j.value("useTexture", true);
        e.usePBR = j.value("usePBR", e.usePBR);
        e.metallic = j.value("metallic", e.metallic);
        e.roughness = j.value("roughness", e.roughness);
        e.ao = j.value("ao", e.ao);
        e.materialPath = j.value("materialPath", std::string());
        // physics
        e.hasRigidBody = j.value("hasRigidBody", false);
        e.isKinematic = j.value("isKinematic", false);
        e.isStatic = j.value("isStatic", false);
        e.mass = j.value("mass", 1.0f);
        e.friction = j.value("friction", 0.5f);
        e.restitution = j.value("restitution", 0.1f);
        auto ch = j.value("colliderHalf", std::vector<float>{0.5f,0.5f,0.5f});
        if (ch.size() == 3) { const_cast<float&>(e.colliderHalf[0]) = ch[0]; const_cast<float&>(e.colliderHalf[1]) = ch[1]; const_cast<float&>(e.colliderHalf[2]) = ch[2]; }
    }

    bool SceneSerializer::save(const Scene& scene, const std::string& path)
    {
        json j;
        json arr = json::array();
        for (const auto& e : scene.getEntities())
            arr.push_back(entityToJson(e));
        j["entities"] = arr;
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;
        f << j.dump(2);
        return true;
    }

    bool SceneSerializer::load(Scene& scene, const std::string& path)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        json j; f >> j;
        scene.entities().clear();
        if (!j.contains("entities")) return true;
        for (const auto& je : j["entities"])
        {
            Entity e{};
            jsonToEntity(je, e);
            // if materialPath set, try to load and bind via a temporary ResourceManager
            if (!e.materialPath.empty())
            {
                // Note: Application owns a ResourceManager; here we don't have it.
                // Leave material binding to runtime; we only keep path in entity.
            }
            scene.entities().push_back(e);
        }
        scene.setSelectedIndex(scene.getEntities().empty() ? -1 : 0);
        return true;
    }
}


