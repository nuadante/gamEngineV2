#include "render/Material.h"
#include "core/ResourceManager.h"
#include "render/Texture2D.h"

#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace engine
{
    static std::string dirOf(const std::string& p)
    {
        size_t pos = p.find_last_of("/\\");
        if (pos == std::string::npos) return std::string();
        return p.substr(0, pos);
    }

    static std::string joinPath(const std::string& base, const std::string& rel)
    {
        if (rel.empty()) return std::string();
        if (rel.size() > 1 && (rel[1] == ':' || rel[0] == '/' || rel[0] == '\\')) return rel; // absolute
        if (base.empty()) return rel;
        char sep = '\\';
        if (!base.empty() && (base.back() == '/' || base.back() == '\\'))
            return base + rel;
        return base + sep + rel;
    }

    bool MaterialAsset::loadFromFile(const std::string& path, ResourceManager* res)
    {
        sourcePath = path;
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        json j; f >> j;
        if (j.contains("albedo") && j["albedo"].is_array() && j["albedo"].size()==3)
        {
            albedo[0] = j["albedo"][0]; albedo[1] = j["albedo"][1]; albedo[2] = j["albedo"][2];
        }
        metallic = j.value("metallic", metallic);
        roughness = j.value("roughness", roughness);
        ao = j.value("ao", ao);
        albedoTexPath = j.value("albedoTex", std::string());
        metallicTexPath = j.value("metallicTex", std::string());
        roughnessTexPath = j.value("roughnessTex", std::string());
        aoTexPath = j.value("aoTex", std::string());
        normalTexPath = j.value("normalTex", std::string());

        std::string base = dirOf(path);
        if (res)
        {
            if (!albedoTexPath.empty()) albedoTex = res->getTextureFromFile(joinPath(base, albedoTexPath), true);
            if (!metallicTexPath.empty()) metallicTex = res->getTextureFromFile(joinPath(base, metallicTexPath), false);
            if (!roughnessTexPath.empty()) roughnessTex = res->getTextureFromFile(joinPath(base, roughnessTexPath), false);
            if (!aoTexPath.empty()) aoTex = res->getTextureFromFile(joinPath(base, aoTexPath), false);
            if (!normalTexPath.empty()) normalTex = res->getTextureFromFile(joinPath(base, normalTexPath), false);
        }
        return true;
    }

    bool MaterialAsset::saveToFile(const std::string& path) const
    {
        json j;
        j["albedo"] = { albedo[0], albedo[1], albedo[2] };
        j["metallic"] = metallic;
        j["roughness"] = roughness;
        j["ao"] = ao;
        if (!albedoTexPath.empty()) j["albedoTex"] = albedoTexPath;
        if (!metallicTexPath.empty()) j["metallicTex"] = metallicTexPath;
        if (!roughnessTexPath.empty()) j["roughnessTex"] = roughnessTexPath;
        if (!aoTexPath.empty()) j["aoTex"] = aoTexPath;
        if (!normalTexPath.empty()) j["normalTex"] = normalTexPath;
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;
        f << j.dump(2);
        return true;
    }
}



