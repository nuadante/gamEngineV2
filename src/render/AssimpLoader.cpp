#include "render/AssimpLoader.h"
#include "render/Mesh.h"
#include "render/Texture2D.h"
#include "render/SkinnedMesh.h"
#include "render/Skeleton.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <map>
#include <set>

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

    static glm::mat4 toGlm(const aiMatrix4x4& m)
    {
        glm::mat4 r;
        r[0][0] = m.a1; r[1][0] = m.a2; r[2][0] = m.a3; r[3][0] = m.a4;
        r[0][1] = m.b1; r[1][1] = m.b2; r[2][1] = m.b3; r[3][1] = m.b4;
        r[0][2] = m.c1; r[1][2] = m.c2; r[2][2] = m.c3; r[3][2] = m.c4;
        r[0][3] = m.d1; r[1][3] = m.d2; r[2][3] = m.d3; r[3][3] = m.d4;
        return r;
    }

    static aiNode* findNode(aiNode* n, const std::string& name)
    {
        if (!n) return nullptr;
        if (n->mName.C_Str() == name) return n;
        for (unsigned i = 0; i < n->mNumChildren; ++i)
            if (auto* r = findNode(n->mChildren[i], name)) return r;
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

    static void sampleChannel(const aiNodeAnim* ch, float t, aiVector3D& pos, aiQuaternion& rot, aiVector3D& sca)
    {
        // Position
        if (ch->mNumPositionKeys > 0)
        {
            unsigned i1 = 0;
            while (i1 + 1 < ch->mNumPositionKeys && t >= (float)ch->mPositionKeys[i1 + 1].mTime) ++i1;
            unsigned i2 = std::min(i1 + 1, ch->mNumPositionKeys - 1);
            float t1 = (float)ch->mPositionKeys[i1].mTime;
            float t2 = (float)ch->mPositionKeys[i2].mTime;
            float a = (t2 > t1) ? (t - t1) / (t2 - t1) : 0.0f;
            pos = ch->mPositionKeys[i1].mValue * (1.0f - a) + ch->mPositionKeys[i2].mValue * a;
        }
        else pos = aiVector3D(0, 0, 0);
        // Rotation
        if (ch->mNumRotationKeys > 0)
        {
            unsigned i1 = 0;
            while (i1 + 1 < ch->mNumRotationKeys && t >= (float)ch->mRotationKeys[i1 + 1].mTime) ++i1;
            unsigned i2 = std::min(i1 + 1, ch->mNumRotationKeys - 1);
            float t1 = (float)ch->mRotationKeys[i1].mTime;
            float t2 = (float)ch->mRotationKeys[i2].mTime;
            float a = (t2 > t1) ? (t - t1) / (t2 - t1) : 0.0f;
            aiQuaternion r1 = ch->mRotationKeys[i1].mValue;
            aiQuaternion r2 = ch->mRotationKeys[i2].mValue;
            aiQuaternion::Interpolate(rot, r1, r2, a);
            rot.Normalize();
        }
        else rot = aiQuaternion();
        // Scale
        if (ch->mNumScalingKeys > 0)
        {
            unsigned i1 = 0;
            while (i1 + 1 < ch->mNumScalingKeys && t >= (float)ch->mScalingKeys[i1 + 1].mTime) ++i1;
            unsigned i2 = std::min(i1 + 1, ch->mNumScalingKeys - 1);
            float t1 = (float)ch->mScalingKeys[i1].mTime;
            float t2 = (float)ch->mScalingKeys[i2].mTime;
            float a = (t2 > t1) ? (t - t1) / (t2 - t1) : 0.0f;
            sca = ch->mScalingKeys[i1].mValue * (1.0f - a) + ch->mScalingKeys[i2].mValue * a;
        }
        else sca = aiVector3D(1, 1, 1);
    }

    bool AssimpLoader::loadSkinned(const std::string& path, ImportedSkinned& outSkinned, bool flipUVs)
    {
        Assimp::Importer importer;
        unsigned flags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights;
        if (flipUVs) flags |= aiProcess_FlipUVs;
        const aiScene* scene = importer.ReadFile(path, flags);
        if (!scene || !scene->mRootNode) return false;

        // Choose first mesh that has bones
        const aiMesh* am = nullptr;
        for (unsigned i = 0; i < scene->mNumMeshes; ++i)
        {
            if (scene->mMeshes[i]->HasBones()) { am = scene->mMeshes[i]; break; }
        }
        if (!am) return false;

        // Build bone name -> index
        std::map<std::string, unsigned> boneIndex;
        for (unsigned b = 0; b < am->mNumBones; ++b)
            boneIndex[am->mBones[b]->mName.C_Str()] = b;

        // Build skeleton
        Skeleton* skel = new Skeleton();
        skel->resize((int)am->mNumBones);
        for (unsigned b = 0; b < am->mNumBones; ++b)
        {
            const aiBone* ab = am->mBones[b];
            skel->bones[b].name = ab->mName.C_Str();
            skel->bones[b].offset = toGlm(ab->mOffsetMatrix);
            // parent by node hierarchy of same name if parent also a bone
            aiNode* node = findNode(scene->mRootNode, skel->bones[b].name);
            int parent = -1;
            if (node && node->mParent)
            {
                std::string pname = node->mParent->mName.C_Str();
                auto it = boneIndex.find(pname);
                if (it != boneIndex.end()) parent = (int)it->second;
            }
            skel->bones[b].parent = parent;
        }

        // Vertex buffers
        std::vector<float> vPNU; vPNU.reserve(am->mNumVertices * 8);
        for (unsigned i = 0; i < am->mNumVertices; ++i)
        {
            aiVector3D p = am->mVertices[i];
            aiVector3D n = am->mNormals ? am->mNormals[i] : aiVector3D(0,1,0);
            aiVector3D t = (am->mTextureCoords[0]) ? am->mTextureCoords[0][i] : aiVector3D(0,0,0);
            vPNU.push_back(p.x); vPNU.push_back(p.y); vPNU.push_back(p.z);
            vPNU.push_back(n.x); vPNU.push_back(n.y); vPNU.push_back(n.z);
            vPNU.push_back(t.x); vPNU.push_back(t.y);
        }
        std::vector<unsigned> indices; indices.reserve(am->mNumFaces * 3);
        for (unsigned f = 0; f < am->mNumFaces; ++f)
            for (unsigned j = 0; j < am->mFaces[f].mNumIndices; ++j) indices.push_back(am->mFaces[f].mIndices[j]);

        std::vector<unsigned> boneIds(am->mNumVertices * 4, 0);
        std::vector<float> weights(am->mNumVertices * 4, 0.0f);
        for (unsigned b = 0; b < am->mNumBones; ++b)
        {
            const aiBone* ab = am->mBones[b];
            for (unsigned w = 0; w < ab->mNumWeights; ++w)
            {
                unsigned v = ab->mWeights[w].mVertexId;
                float val = ab->mWeights[w].mWeight;
                // insert to smallest weight slot
                unsigned base = v * 4;
                int slot = -1; float minW = 1e9f; int minIdx = -1;
                for (int k = 0; k < 4; ++k)
                {
                    if (weights[base + k] == 0.0f && slot == -1) slot = k;
                    if (weights[base + k] < minW) { minW = weights[base + k]; minIdx = k; }
                }
                if (slot == -1) slot = minIdx;
                boneIds[base + slot] = b;
                weights[base + slot] = val;
            }
        }
        // normalize weights per vertex
        for (unsigned v = 0; v < am->mNumVertices; ++v)
        {
            unsigned base = v * 4; float s = weights[base] + weights[base+1] + weights[base+2] + weights[base+3];
            if (s > 0.0f) { weights[base]/=s; weights[base+1]/=s; weights[base+2]/=s; weights[base+3]/=s; }
            else { weights[base]=1.0f; }
        }

        SkinnedMesh* sm = new SkinnedMesh();
        if (!sm->create(vPNU, boneIds, weights, indices)) { delete sm; delete skel; return false; }

        // Materials (diffuse)
        Texture2D* diff = nullptr;
        if (am->mMaterialIndex < scene->mNumMaterials)
        {
            std::string baseDir = path; size_t p = baseDir.find_last_of("/\\"); baseDir = (p == std::string::npos) ? std::string("") : baseDir.substr(0, p);
            const aiMaterial* mat = scene->mMaterials[am->mMaterialIndex];
            diff = loadMaterialTexture(mat, aiTextureType_DIFFUSE, baseDir);
        }

        // Animations (sampled frames)
        std::vector<Animation>* anims = new std::vector<Animation>();
        for (unsigned a = 0; a < scene->mNumAnimations; ++a)
        {
            const aiAnimation* aa = scene->mAnimations[a];
            Animation A; A.name = aa->mName.C_Str(); A.duration = (float)aa->mDuration; A.ticksPerSecond = (float)(aa->mTicksPerSecond != 0.0 ? aa->mTicksPerSecond : 25.0);
            std::map<std::string, const aiNodeAnim*> chanOf;
            for (unsigned c = 0; c < aa->mNumChannels; ++c) chanOf[aa->mChannels[c]->mNodeName.C_Str()] = aa->mChannels[c];
            // gather times
            std::set<float> times; times.insert(0.0f); times.insert((float)aa->mDuration);
            for (auto& kv : chanOf)
            {
                const aiNodeAnim* ch = kv.second;
                for (unsigned i = 0; i < ch->mNumPositionKeys; ++i) times.insert((float)ch->mPositionKeys[i].mTime);
                for (unsigned i = 0; i < ch->mNumRotationKeys; ++i) times.insert((float)ch->mRotationKeys[i].mTime);
                for (unsigned i = 0; i < ch->mNumScalingKeys; ++i) times.insert((float)ch->mScalingKeys[i].mTime);
            }
            A.frames.reserve(times.size());
            for (float t : times)
            {
                Keyframe kf; kf.time = t; kf.localTransforms.resize(skel->bones.size(), glm::mat4(1.0f));
                for (size_t b = 0; b < skel->bones.size(); ++b)
                {
                    const std::string& bn = skel->bones[b].name;
                    auto it = chanOf.find(bn);
                    if (it != chanOf.end())
                    {
                        aiVector3D pos, sca; aiQuaternion rot;
                        sampleChannel(it->second, t, pos, rot, sca);
                        glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, pos.y, pos.z));
                        glm::mat4 R = glm::mat4_cast(glm::quat(rot.w, rot.x, rot.y, rot.z));
                        glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(sca.x, sca.y, sca.z));
                        kf.localTransforms[b] = T * R * S;
                    }
                }
                A.frames.push_back(std::move(kf));
            }
            anims->push_back(std::move(A));
        }

        outSkinned.mesh = sm;
        outSkinned.diffuse = diff;
        outSkinned.skeleton = skel;
        outSkinned.animations = anims;
        outSkinned.name = am->mName.C_Str();
        return true;
    }
}


