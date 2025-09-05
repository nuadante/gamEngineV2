#include "physics/Physics.h"

#include <PxPhysicsAPI.h>

using namespace physx;

namespace engine
{
    Physics::Physics() = default;
    Physics::~Physics()
    {
        shutdown();
    }

    bool Physics::initialize()
    {
        static PxDefaultAllocator      gAllocator;
        static PxDefaultErrorCallback  gErrorCallback;

        m_foundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, gErrorCallback);
        if (!m_foundation) return false;

        // No PVD connection for simplicity
        m_pvd = nullptr;

        // Initialize PhysX and Extensions
        m_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_foundation, PxTolerancesScale(), true, m_pvd);
        if (!m_physics) return false;

        if (!PxInitExtensions(*m_physics, m_pvd))
            return false;

        return true;
    }

    void Physics::shutdown()
    {
        if (m_scene) { m_scene->release(); m_scene = nullptr; }
        if (m_dispatcher) { m_dispatcher->release(); m_dispatcher = nullptr; }
        if (m_physics) { PxCloseExtensions(); m_physics->release(); m_physics = nullptr; }
        if (m_pvd) { m_pvd->release(); m_pvd = nullptr; }
        if (m_foundation) { m_foundation->release(); m_foundation = nullptr; }
        m_defaultMaterial = nullptr;
    }

    bool Physics::createDefaultScene(float gravityY)
    {
        PxSceneDesc sceneDesc(m_physics->getTolerancesScale());
        sceneDesc.gravity = PxVec3(0.0f, gravityY, 0.0f);
        m_dispatcher = PxDefaultCpuDispatcherCreate(2);
        sceneDesc.cpuDispatcher = m_dispatcher;
        sceneDesc.filterShader = PxDefaultSimulationFilterShader;
        m_scene = m_physics->createScene(sceneDesc);
        if (!m_scene) return false;

        m_defaultMaterial = m_physics->createMaterial(0.5f, 0.5f, 0.6f);
        if (!m_defaultMaterial) return false;
        return true;
    }

    void Physics::simulate(float deltaTimeSeconds)
    {
        if (!m_scene) return;
        m_scene->simulate(deltaTimeSeconds);
        m_scene->fetchResults(true);
    }

    PxMaterial* Physics::defaultMaterial()
    {
        return m_defaultMaterial;
    }

    PxRigidStatic* Physics::addGroundPlane()
    {
        if (!m_scene || !m_physics || !m_defaultMaterial) return nullptr;
        PxRigidStatic* groundPlane = PxCreatePlane(*m_physics, PxPlane(0, 1, 0, 0), *m_defaultMaterial);
        if (groundPlane) m_scene->addActor(*groundPlane);
        return groundPlane;
    }

    PxRigidDynamic* Physics::addDynamicBox(float x, float y, float z, float hx, float hy, float hz, float density)
    {
        if (!m_scene || !m_physics || !m_defaultMaterial) return nullptr;
        PxTransform pose(PxVec3(x, y, z));
        PxBoxGeometry boxGeo(hx, hy, hz);
        PxRigidDynamic* body = PxCreateDynamic(*m_physics, pose, boxGeo, *m_defaultMaterial, density);
        if (!body) return nullptr;
        body->setAngularDamping(0.5f);
        m_scene->addActor(*body);
        return body;
    }
}


