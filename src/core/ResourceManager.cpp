#include "core/ResourceManager.h"
#include "render/Shader.h"
#include "render/Texture2D.h"
#include "render/Mesh.h"
#include "render/Material.h"

namespace engine
{
    ResourceManager::~ResourceManager() = default;
    Shader* ResourceManager::getShaderFromSource(const std::string& key, const std::string& vertexSrc, const std::string& fragmentSrc)
    {
        auto it = m_shaders.find(key);
        if (it != m_shaders.end()) return it->second.get();
        auto shader = std::make_unique<Shader>();
        if (!shader->compileFromSource(vertexSrc, fragmentSrc))
            return nullptr;
        Shader* ptr = shader.get();
        m_shaders[key] = std::move(shader);
        return ptr;
    }

    Texture2D* ResourceManager::getTextureFromFile(const std::string& path, bool flipY)
    {
        auto it = m_textures.find(path);
        if (it != m_textures.end()) return it->second.get();
        auto tex = std::make_unique<Texture2D>();
        if (!tex->loadFromFile(path, flipY))
            return nullptr;
        Texture2D* ptr = tex.get();
        m_textures[path] = std::move(tex);
        m_texturePathByPtr[ptr] = path;
        return ptr;
    }

    Texture2D* ResourceManager::getCheckerboard(const std::string& key, int width, int height, int tileSize)
    {
        auto it = m_textures.find(key);
        if (it != m_textures.end()) return it->second.get();
        auto tex = std::make_unique<Texture2D>();
        tex->createCheckerboard(width, height, tileSize);
        Texture2D* ptr = tex.get();
        m_textures[key] = std::move(tex);
        return ptr;
    }

    Mesh* ResourceManager::getCube(const std::string& key)
    {
        auto it = m_meshes.find(key);
        if (it != m_meshes.end()) return it->second.get();
        auto mesh = std::make_unique<Mesh>(Mesh::createCube());
        Mesh* ptr = mesh.get();
        m_meshes[key] = std::move(mesh);
        return ptr;
    }

    Mesh* ResourceManager::getPlane(const std::string& key)
    {
        auto it = m_meshes.find(key);
        if (it != m_meshes.end()) return it->second.get();
        auto mesh = std::make_unique<Mesh>(Mesh::createPlane());
        Mesh* ptr = mesh.get();
        m_meshes[key] = std::move(mesh);
        return ptr;
    }

    MaterialAsset* ResourceManager::getMaterialFromFile(const std::string& path)
    {
        auto it = m_materials.find(path);
        if (it != m_materials.end()) return &it->second;
        MaterialAsset mat;
        if (!mat.loadFromFile(path, this))
            return nullptr;
        m_materials.emplace(path, std::move(mat));
        return &m_materials[path];
    }

    const std::string* ResourceManager::getPathForTexture(const Texture2D* tex) const
    {
        auto it = m_texturePathByPtr.find(tex);
        if (it == m_texturePathByPtr.end()) return nullptr;
        return &it->second;
    }
}


