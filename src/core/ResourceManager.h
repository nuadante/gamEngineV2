#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include "render/Material.h"
#include "render/Shader.h"
#include "render/Texture2D.h"
#include "render/Mesh.h"

namespace engine
{
    class Shader;
    class Texture2D;
    class Mesh;
    struct MaterialAsset;

    class ResourceManager
    {
    public:
        ResourceManager() = default;
        ~ResourceManager();

        Shader* getShaderFromSource(const std::string& key, const std::string& vertexSrc, const std::string& fragmentSrc);
        Texture2D* getTextureFromFile(const std::string& path, bool flipY);
        Texture2D* getCheckerboard(const std::string& key, int width, int height, int tileSize);
        Mesh* getCube(const std::string& key);
        Mesh* getPlane(const std::string& key);
        // material asset cache by path
        MaterialAsset* getMaterialFromFile(const std::string& path);
        // reverse lookup helpers
        const std::string* getPathForTexture(const Texture2D* tex) const;

    private:
        std::unordered_map<std::string, std::unique_ptr<Shader>> m_shaders;
        std::unordered_map<std::string, std::unique_ptr<Texture2D>> m_textures;
        std::unordered_map<const Texture2D*, std::string> m_texturePathByPtr;
        std::unordered_map<std::string, std::unique_ptr<Mesh>> m_meshes;
        std::unordered_map<std::string, MaterialAsset> m_materials;
    };
}


