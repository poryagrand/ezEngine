#include <PhysXPlugin/PCH.h>
#include <PhysXPlugin/PhysXSceneModule.h>
#include <Core/World/World.h>

#include <PhysXPlugin/Components/PxStaticActorComponent.h>
#include <PhysXPlugin/Components/PxDynamicActorComponent.h>
#include <PhysXPlugin/Shapes/PxShapeBoxComponent.h>
#include <PhysXPlugin/Shapes/PxShapeSphereComponent.h>
#include <PhysXPlugin/Shapes/PxShapeCapsuleComponent.h>
#include <PhysXPlugin/Shapes/PxShapeConvexComponent.h>
#include <PhysXPlugin/Components/PxCenterOfMassComponent.h>
#include <PhysXPlugin/Joints/PxDistanceJointComponent.h>
#include <PhysXPlugin/Joints/PxFixedJointComponent.h>

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezPhysXSceneModule, 1, ezRTTIDefaultAllocator<ezPhysXSceneModule>);
  // no properties or message handlers
EZ_END_DYNAMIC_REFLECTED_TYPE();

PxFilterFlags ezPxFilterShader(PxFilterObjectAttributes attributes0, PxFilterData filterData0, PxFilterObjectAttributes attributes1, PxFilterData filterData1, PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize)
{
  // let triggers through
  if (PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1))
  {
    pairFlags = PxPairFlag::eTRIGGER_DEFAULT;
    return PxFilterFlag::eDEFAULT;
  }

  pairFlags = (PxPairFlag::Enum)0;

  // trigger the contact callback for pairs (A,B) where
  // the filter mask of A contains the ID of B and vice versa.
  if ((filterData0.word0 & filterData1.word1) || (filterData1.word0 & filterData0.word1))
  {
    pairFlags |= PxPairFlag::eCONTACT_DEFAULT;
    return PxFilterFlag::eDEFAULT;
  }

  return PxFilterFlag::eKILL;
}

void ezPhysXSceneModule::InternalStartup()
{
  InternalReinit();

  GetWorld()->CreateComponentManager<ezPxStaticActorComponentManager>()->SetUserData(this);
  GetWorld()->CreateComponentManager<ezPxDynamicActorComponentManager>()->SetUserData(this);
  GetWorld()->CreateComponentManager<ezPxShapeBoxComponentManager>()->SetUserData(this);
  GetWorld()->CreateComponentManager<ezPxShapeSphereComponentManager>()->SetUserData(this);
  GetWorld()->CreateComponentManager<ezPxShapeCapsuleComponentManager>()->SetUserData(this);
  GetWorld()->CreateComponentManager<ezPxShapeConvexComponentManager>()->SetUserData(this);
  GetWorld()->CreateComponentManager<ezPxCenterOfMassComponentManager>()->SetUserData(this);
  GetWorld()->CreateComponentManager<ezPxDistanceJointComponentManager>()->SetUserData(this);
  GetWorld()->CreateComponentManager<ezPxFixedJointComponentManager>()->SetUserData(this);

  m_AccumulatedTimeSinceUpdate.SetZero();

  PxSceneDesc desc = PxSceneDesc(PxTolerancesScale());
  desc.setToDefault(PxTolerancesScale());

  desc.gravity = PxVec3(0.0f, 0.0f, -9.81f);

  m_pCPUDispatcher = PxDefaultCpuDispatcherCreate(4);
  desc.cpuDispatcher = m_pCPUDispatcher;
  desc.filterShader = ezPxFilterShader;

  EZ_ASSERT_DEV(desc.isValid(), "PhysX scene description is invalid");
  m_pPxScene = ezPhysX::GetSingleton()->GetPhysXAPI()->createScene(desc);

  EZ_ASSERT_ALWAYS(m_pPxScene != nullptr, "Creating the PhysX scene failed");
}

void ezPhysXSceneModule::InternalShutdown()
{
  m_pPxScene->release();
  m_pPxScene = nullptr;

  m_pCPUDispatcher->release();
  m_pCPUDispatcher = nullptr;
}

void ezPhysXSceneModule::InternalUpdate()
{
  if (!GetWorld()->GetWorldSimulationEnabled())
    return;

  ezTime tDiff = GetWorld()->GetClock().GetTimeDiff();
  m_AccumulatedTimeSinceUpdate += tDiff;

  const ezTime tStep = ezTime::Seconds(1.0 / 60.0);

  while (m_AccumulatedTimeSinceUpdate >= tStep)
  {
    m_pPxScene->simulate((PxReal) tStep.GetSeconds());
    m_pPxScene->fetchResults(true);

    m_AccumulatedTimeSinceUpdate -= tStep;
  }
}


void ezPhysXSceneModule::InternalReinit()
{
  ezPhysX::GetSingleton()->LoadCollisionFilters();

}

void ezPxAllocatorCallback::VerifyAllocations()
{
#if EZ_ENABLED(EZ_COMPILE_FOR_DEBUG)
  EZ_ASSERT_DEV(m_Allocations.IsEmpty(), "There are %u unfreed allocations", m_Allocations.GetCount());

  for (auto it = m_Allocations.GetIterator(); it.IsValid(); ++it)
  {
    const char* s = it.Value().GetData();
    ezLog::Info(s);
  }
#endif
}
