#pragma once

#include <vector>
#include <string>
#include <glm/mat4x4.hpp>

namespace engine
{
    struct Bone
    {
        std::string name;
        int parent = -1;
        glm::mat4 offset = glm::mat4(1.0f); // inverse bind pose
        glm::mat4 localBind = glm::mat4(1.0f);
    };

    struct Keyframe
    {
        float time = 0.0f;
        std::vector<glm::mat4> localTransforms; // per bone
    };

    struct Animation
    {
        std::string name;
        float duration = 0.0f;
        float ticksPerSecond = 25.0f;
        std::vector<Keyframe> frames;
    };

    class Skeleton
    {
    public:
        std::vector<Bone> bones;
        std::vector<glm::mat4> posePalette; // final skinning matrices

        void resize(int count)
        {
            bones.resize(count);
            posePalette.resize(count);
        }
    };
}


