#pragma once

#include <string>
#include <unordered_map>
#include <memory>

namespace engine
{
    class Shader;
    class Texture2D;
    class Mesh;

    class ResourceManager
    {
    public:
        ResourceManager() = default;
        ~ResourceManager() = default;

        Shader* getShaderFromSource(const std::string& key, const std::string& vertexSrc, const std::string& fragmentSrc);
        Texture2D* getTextureFromFile(const std::string& path, bool flipY);
        Texture2D* getCheckerboard(const std::string& key, int width, int height, int tileSize);
        Mesh* getCube(const std::string& key);
        Mesh* getPlane(const std::string& key);

    private:
        std::unordered_map<std::string, std::unique_ptr<Shader>> m_shaders;
        std::unordered_map<std::string, std::unique_ptr<Texture2D>> m_textures;
        std::unordered_map<std::string, std::unique_ptr<Mesh>> m_meshes;
    };
}


