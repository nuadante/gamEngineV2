#include "ecs/ECSSerializer.h"
#include "ecs/ECS.h"
#include "core/ResourceManager.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>
#include <string>

using json = nlohmann::json;

namespace engine
{
    static json transformToJson(const TransformC& t)
    {
        json j;
        j["position"] = { t.position.x, t.position.y, t.position.z };
        j["rotationEuler"] = { t.rotationEuler.x, t.rotationEuler.y, t.rotationEuler.z };
        j["scale"] = { t.scale.x, t.scale.y, t.scale.z };
        return j;
    }
    static void jsonToTransform(const json& j, TransformC& t)
    {
        auto p = j.value("position", std::vector<float>{0,0,0});
        auto r = j.value("rotationEuler", std::vector<float>{0,0,0});
        auto s = j.value("scale", std::vector<float>{1,1,1});
        t.position = { p[0], p[1], p[2] };
        t.rotationEuler = { r[0], r[1], r[2] };
        t.scale = { s[0], s[1], s[2] };
    }
    static json meshRendererToJson(const MeshRendererC& mr)
    {
        json j;
        j["usePBR"] = mr.usePBR;
        if (!mr.materialPath.empty()) j["materialPath"] = mr.materialPath;
        // Note: Mesh* ve Texture2D* pointerlari serialize edilmez; materyal asset yollari yeterli
        return j;
    }
    static void jsonToMeshRenderer(const json& j, MeshRendererC& mr, ResourceManager* resources)
    {
        mr.usePBR = j.value("usePBR", mr.usePBR);
        mr.materialPath = j.value("materialPath", std::string());
        if (!mr.materialPath.empty() && resources)
        {
            if (auto* mat = resources->getMaterialFromFile(mr.materialPath))
            {
                mr.material = mat;
                if (mat->albedoTex) mr.albedoTex = mat->albedoTex;
                mr.usePBR = true;
            }
        }
    }
    static json dirLightToJson(const DirectionalLightC& dl)
    {
        json j; j["color"]={dl.color.x,dl.color.y,dl.color.z}; j["intensity"]=dl.intensity; j["direction"]={dl.direction.x,dl.direction.y,dl.direction.z}; return j;
    }
    static void jsonToDirLight(const json& j, DirectionalLightC& dl)
    {
        auto c=j.value("color", std::vector<float>{1,1,1}); dl.color={c[0],c[1],c[2]}; dl.intensity=j.value("intensity",1.0f); auto d=j.value("direction", std::vector<float>{-0.5f,-1.0f,-0.3f}); dl.direction={d[0],d[1],d[2]};
    }
    static json pointLightToJson(const PointLightC& pl)
    {
        json j; j["color"]={pl.color.x,pl.color.y,pl.color.z}; j["intensity"]=pl.intensity; j["range"]=pl.range; return j;
    }
    static void jsonToPointLight(const json& j, PointLightC& pl)
    {
        auto c=j.value("color", std::vector<float>{1,1,1}); pl.color={c[0],c[1],c[2]}; pl.intensity=j.value("intensity",1.0f); pl.range=j.value("range",25.0f);
    }
    static json spotLightToJson(const SpotLightC& sl)
    {
        json j; j["color"]={sl.color.x,sl.color.y,sl.color.z}; j["intensity"]=sl.intensity; j["direction"]={sl.direction.x,sl.direction.y,sl.direction.z}; j["inner"]=sl.innerDegrees; j["outer"]=sl.outerDegrees; j["near"]=sl.nearPlane; j["far"]=sl.farPlane; return j;
    }
    static void jsonToSpotLight(const json& j, SpotLightC& sl)
    {
        auto c=j.value("color", std::vector<float>{1,1,1}); sl.color={c[0],c[1],c[2]}; sl.intensity=j.value("intensity",1.0f); auto d=j.value("direction", std::vector<float>{0,-1,0}); sl.direction={d[0],d[1],d[2]}; sl.innerDegrees=j.value("inner",15.0f); sl.outerDegrees=j.value("outer",25.0f); sl.nearPlane=j.value("near",0.1f); sl.farPlane=j.value("far",40.0f);
    }
    static json particleToJson(const ParticleEmitterC& pe)
    {
        json j; j["emit"]=pe.emit; j["rate"]=pe.rate; return j;
    }
    static void jsonToParticle(const json& j, ParticleEmitterC& pe)
    {
        pe.emit=j.value("emit",true); pe.rate=j.value("rate",50.0f);
    }
    static json rigidBodyToJson(const RigidBodyC& rb)
    {
        json j; j["isStatic"]=rb.isStatic; j["isKinematic"]=rb.isKinematic; j["mass"]=rb.mass; j["friction"]=rb.friction; j["restitution"]=rb.restitution; return j;
    }
    static void jsonToRigidBody(const json& j, RigidBodyC& rb)
    {
        rb.isStatic=j.value("isStatic", false); rb.isKinematic=j.value("isKinematic", false); rb.mass=j.value("mass",1.0f); rb.friction=j.value("friction",0.5f); rb.restitution=j.value("restitution",0.1f);
    }
    static json boxColliderToJson(const BoxColliderC& bc)
    {
        json j; j["half"] = { bc.hx, bc.hy, bc.hz }; return j;
    }
    static void jsonToBoxCollider(const json& j, BoxColliderC& bc)
    {
        auto h = j.value("half", std::vector<float>{0.5f,0.5f,0.5f});
        bc.hx = h[0]; bc.hy = h[1]; bc.hz = h[2];
    }

    bool ECSSerializer::save(ECS& ecs, const std::string& path)
    {
        json root; root["entities"] = json::array();
        auto& reg = ecs.registry;
        auto view = reg.view<TagC>();
        for (auto e : view)
        {
            json je;
            je["name"] = view.get<TagC>(e).name;
            if (auto tr = reg.try_get<TransformC>(e)) je["transform"] = transformToJson(*tr);
            if (auto mr = reg.try_get<MeshRendererC>(e)) je["meshRenderer"] = meshRendererToJson(*mr);
            if (auto dl = reg.try_get<DirectionalLightC>(e)) je["dirLight"] = dirLightToJson(*dl);
            if (auto pl = reg.try_get<PointLightC>(e)) je["pointLight"] = pointLightToJson(*pl);
            if (auto sl = reg.try_get<SpotLightC>(e)) je["spotLight"] = spotLightToJson(*sl);
            if (auto pe = reg.try_get<ParticleEmitterC>(e)) je["particle"] = particleToJson(*pe);
            if (auto rb = reg.try_get<RigidBodyC>(e)) je["rigidBody"] = rigidBodyToJson(*rb);
            if (auto bc = reg.try_get<BoxColliderC>(e)) je["boxCollider"] = boxColliderToJson(*bc);
            root["entities"].push_back(std::move(je));
        }
        std::ofstream f(path, std::ios::binary); if (!f) return false; f << root.dump(2); return true;
    }

    bool ECSSerializer::load(ECS& ecs, const std::string& path, ResourceManager* resources)
    {
        std::ifstream f(path, std::ios::binary); if (!f) return false; json root; f >> root;
        ecs.registry.clear();
        if (!root.contains("entities")) return true;
        for (const auto& je : root["entities"])
        {
            std::string name = je.value("name", std::string("Entity"));
            entt::entity e = ecs.registry.create();
            ecs.registry.emplace<TagC>(e, name);
            ecs.registry.emplace<TransformC>(e);
            if (je.contains("transform")) jsonToTransform(je["transform"], ecs.registry.get<TransformC>(e));
            if (je.contains("meshRenderer")) { auto& mr = ecs.registry.emplace<MeshRendererC>(e); jsonToMeshRenderer(je["meshRenderer"], mr, resources); }
            if (je.contains("dirLight")) { auto& dl = ecs.registry.emplace<DirectionalLightC>(e); jsonToDirLight(je["dirLight"], dl); }
            if (je.contains("pointLight")) { auto& pl = ecs.registry.emplace<PointLightC>(e); jsonToPointLight(je["pointLight"], pl); }
            if (je.contains("spotLight")) { auto& sl = ecs.registry.emplace<SpotLightC>(e); jsonToSpotLight(je["spotLight"], sl); }
            if (je.contains("particle")) { auto& pe = ecs.registry.emplace<ParticleEmitterC>(e); jsonToParticle(je["particle"], pe); }
            if (je.contains("rigidBody")) { auto& rb = ecs.registry.emplace<RigidBodyC>(e); jsonToRigidBody(je["rigidBody"], rb); }
            if (je.contains("boxCollider")) { auto& bc = ecs.registry.emplace<BoxColliderC>(e); jsonToBoxCollider(je["boxCollider"], bc); }
        }
        return true;
    }
}



