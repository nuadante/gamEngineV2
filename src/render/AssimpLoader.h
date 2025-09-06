#pragma once

#include <string>
#include <vector>

namespace engine
{
    class Mesh;
    class Texture2D;
    class SkinnedMesh;
    struct Animation;
    class Skeleton;

    struct ImportedMesh
    {
        Mesh* mesh;
        Texture2D* diffuse;
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
        static bool loadModel(const std::string& path, std::vector<ImportedMesh>& outMeshes, bool flipUVs);
        static bool loadSkinned(const std::string& path, ImportedSkinned& outSkinned, bool flipUVs);
    };
}


