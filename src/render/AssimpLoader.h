#pragma once

#include <string>
#include <vector>

namespace engine
{
    class Mesh;
    class Texture2D;

    struct ImportedMesh
    {
        Mesh* mesh;
        Texture2D* diffuse;
        std::string name;
    };

    class AssimpLoader
    {
    public:
        static bool loadModel(const std::string& path, std::vector<ImportedMesh>& outMeshes, bool flipUVs);
    };
}


