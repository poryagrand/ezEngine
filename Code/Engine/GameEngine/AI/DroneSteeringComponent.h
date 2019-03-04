#pragma once

#include <GameEngine/GameEngineDLL.h>

#include <Core/World/Component.h>
#include <Core/World/World.h>

class ezPhysicsWorldModuleInterface;

class EZ_GAMEENGINE_DLL ezDroneSteeringComponentManager
  : public ezComponentManager<class ezDroneSteeringComponent, ezBlockStorageType::FreeList>
{
  typedef ezComponentManager<class ezDroneSteeringComponent, ezBlockStorageType::FreeList> SUPER;

public:
  ezDroneSteeringComponentManager(ezWorld* pWorld);
  ~ezDroneSteeringComponentManager();

  virtual void Initialize() override;

  void Update(const ezWorldModule::UpdateContext& context);

  ezPhysicsWorldModuleInterface* m_pPhysicsInterface = nullptr;
};


class EZ_GAMEENGINE_DLL ezDroneSteeringComponent : public ezComponent
{
  EZ_DECLARE_COMPONENT_TYPE(ezDroneSteeringComponent, ezComponent, ezDroneSteeringComponentManager);

public:
  ezDroneSteeringComponent();

  //////////////////////////////////////////////////////////////////////////
  // ezComponent Interface
  //

  virtual void OnSimulationStarted() override;

  virtual void SerializeComponent(ezWorldWriter& stream) const override;
  virtual void DeserializeComponent(ezWorldReader& stream) override;

  //////////////////////////////////////////////////////////////////////////
  // ezDroneSteeringComponent Interface
  //

  void ScanArea(ezPhysicsWorldModuleInterface* pPhysicsInterface);
  void Update();

protected:
  ezVec3 FindBestDirection(ezPhysicsWorldModuleInterface* pPhysicsInterface, const ezVec3& vPos, const ezVec3& vTarget) const;
  float ComputeBestLength(ezPhysicsWorldModuleInterface* pPhysicsInterface, const ezVec3& vStart, const ezVec3& vSideDir, float fSideDist, const ezVec3& vForwardDir) const;

  float m_fSpeedForce = 10.0f;
  float m_fArriveDistance = 2.0f;
  float m_fScanningDistance = 15.0f;
  ezVec3 m_vCurrentTargetPosition;
  ezVec3 m_vLastFlyDir;
  ezTime m_LastUpdate;
};
