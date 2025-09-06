#pragma once

#include <string>

namespace engine
{
    class Texture2D;
    class ResourceManager;

    struct MaterialAsset
    {
        std::string sourcePath; // optional: where it was loaded from
        float albedo[3] = {1.0f, 1.0f, 1.0f};
        float metallic = 0.0f;
        float roughness = 0.5f;
        float ao = 1.0f;

        // Source file paths (can be relative to asset file)
        std::string albedoTexPath;
        std::string metallicTexPath;
        std::string roughnessTexPath;
        std::string aoTexPath;
        std::string normalTexPath;

        // Loaded textures (cached by ResourceManager)
        Texture2D* albedoTex = nullptr;
        Texture2D* metallicTex = nullptr;
        Texture2D* roughnessTex = nullptr;
        Texture2D* aoTex = nullptr;
        Texture2D* normalTex = nullptr;

        bool loadFromFile(const std::string& path, ResourceManager* res);
        bool saveToFile(const std::string& path) const;
    };
}



