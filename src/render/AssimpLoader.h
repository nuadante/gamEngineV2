#pragma once

#include <string>
#include <vector>

namespace engine
{
    class Mesh;
    class Texture2D;
    class ResourceManager;
    class SkinnedMesh;
    struct Animation;
    class Skeleton;

    struct ImportedMesh
    {
        Mesh* mesh;
        Texture2D* diffuse;
        Texture2D* metal = nullptr;
        Texture2D* rough = nullptr;
        Texture2D* ao = nullptr;
        Texture2D* normal = nullptr;
        std::string name;
    };

    struct ImportedSkinned
    {
        SkinnedMesh* mesh = nullptr;
        Texture2D* diffuse = nullptr;
        Skeleton* skeleton = nullptr;
        std::vector<Animation>* animations = nullptr;
        std::string name;
    };

    class AssimpLoader
    {
    public:
        static bool loadModel(class ResourceManager* resources, const std::string& path, std::vector<ImportedMesh>& outMeshes, bool flipUVs);
        static bool loadSkinned(class ResourceManager* resources, const std::string& path, ImportedSkinned& outSkinned, bool flipUVs);
    };
}


