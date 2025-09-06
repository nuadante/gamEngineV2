#include "render/AssimpLoader.h"
#include "render/Mesh.h"
#include "render/Texture2D.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace engine
{
    static Mesh* createMeshFromAi(const aiMesh* m)
    {
        std::vector<float> vertices; vertices.reserve(m->mNumVertices * 8);
        for (unsigned i = 0; i < m->mNumVertices; ++i)
        {
            aiVector3D p = m->mVertices[i];
            aiVector3D n = m->mNormals ? m->mNormals[i] : aiVector3D(0,1,0);
            aiVector3D t = (m->mTextureCoords[0]) ? m->mTextureCoords[0][i] : aiVector3D(0,0,0);
            vertices.push_back(p.x); vertices.push_back(p.y); vertices.push_back(p.z);
            vertices.push_back(n.x); vertices.push_back(n.y); vertices.push_back(n.z);
            vertices.push_back(t.x); vertices.push_back(t.y);
        }
        std::vector<unsigned> indices; indices.reserve(m->mNumFaces * 3);
        for (unsigned f = 0; f < m->mNumFaces; ++f)
        {
            const aiFace& face = m->mFaces[f];
            for (unsigned j = 0; j < face.mNumIndices; ++j) indices.push_back(face.mIndices[j]);
        }
        Mesh* mesh = new Mesh();
        mesh->create(vertices, indices);
        return mesh;
    }

    static Texture2D* loadMaterialTexture(const aiMaterial* mat, aiTextureType type, const std::string& baseDir)
    {
        (void)mat; (void)type; (void)baseDir;
        // TODO: Wire with ResourceManager to load textures; returning nullptr for now.
        return nullptr;
    }

    bool AssimpLoader::loadModel(const std::string& path, std::vector<ImportedMesh>& outMeshes, bool flipUVs)
    {
        Assimp::Importer importer;
        unsigned flags = aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace;
        if (flipUVs) flags |= aiProcess_FlipUVs;
        const aiScene* scene = importer.ReadFile(path, flags);
        if (!scene || !scene->mRootNode) return false;

        // base dir for textures
        std::string baseDir = path;
        size_t p = baseDir.find_last_of("/\\");
        baseDir = (p == std::string::npos) ? std::string("") : baseDir.substr(0, p);

        for (unsigned i = 0; i < scene->mNumMeshes; ++i)
        {
            const aiMesh* am = scene->mMeshes[i];
            Mesh* mesh = createMeshFromAi(am);
            Texture2D* diff = nullptr;
            if (am->mMaterialIndex < scene->mNumMaterials)
            {
                const aiMaterial* mat = scene->mMaterials[am->mMaterialIndex];
                diff = loadMaterialTexture(mat, aiTextureType_DIFFUSE, baseDir);
            }
            ImportedMesh im{ mesh, diff, am->mName.C_Str() };
            outMeshes.push_back(im);
        }
        return !outMeshes.empty();
    }
}


