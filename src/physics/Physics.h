#pragma once

#include <memory>

// Forward PhysX types
namespace physx {
    class PxFoundation;
    class PxPhysics;
    class PxDefaultCpuDispatcher;
    class PxPvd;
    class PxScene;
    class PxMaterial;
    class PxRigidStatic;
    class PxRigidDynamic;
}

namespace engine
{
    class Physics
    {
    public:
        Physics();
        ~Physics();

        bool initialize();
        void shutdown();

        void simulate(float deltaTimeSeconds);

        // Basic helpers
        bool createDefaultScene(float gravityY = -9.81f);
        physx::PxMaterial* defaultMaterial();

        // Create static plane at y=0
        physx::PxRigidStatic* addGroundPlane();
        // Create dynamic box at position with half extents and density
        physx::PxRigidDynamic* addDynamicBox(float x, float y, float z, float hx, float hy, float hz, float density);

        physx::PxScene* scene() const { return m_scene; }

    private:
        physx::PxFoundation* m_foundation = nullptr;
        physx::PxPhysics* m_physics = nullptr;
        physx::PxDefaultCpuDispatcher* m_dispatcher = nullptr;
        physx::PxPvd* m_pvd = nullptr;
        physx::PxScene* m_scene = nullptr;
        physx::PxMaterial* m_defaultMaterial = nullptr;
    };
}


