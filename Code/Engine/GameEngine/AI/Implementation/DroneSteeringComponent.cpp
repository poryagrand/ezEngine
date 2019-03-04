#include <GameEnginePCH.h>

#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <Foundation/Configuration/CVar.h>
#include <GameEngine/AI/DroneSteeringComponent.h>
#include <GameEngine/Components/HeadBoneComponent.h>
#include <GameEngine/Interfaces/PhysicsWorldModule.h>
#include <RendererCore/Debug/DebugRenderer.h>

ezCVarBool cvar_AiVisSteering("ai_VisSteering", false, ezCVarFlags::Save, "Visualizes the AI data and raycasts used for steering.");

ezDroneSteeringComponentManager::ezDroneSteeringComponentManager(ezWorld* pWorld)
  : SUPER(pWorld)
{
}

ezDroneSteeringComponentManager::~ezDroneSteeringComponentManager() = default;

void ezDroneSteeringComponentManager::Initialize()
{
  SUPER::Initialize();

  m_pPhysicsInterface = GetWorld()->GetOrCreateModule<ezPhysicsWorldModuleInterface>();

  auto desc = EZ_CREATE_MODULE_UPDATE_FUNCTION_DESC(ezDroneSteeringComponentManager::Update, this);
  desc.m_bOnlyUpdateWhenSimulating = true;
  desc.m_Phase = UpdateFunctionDesc::Phase::Async;
  desc.m_uiGranularity = 1;

  RegisterUpdateFunction(desc);
}

void ezDroneSteeringComponentManager::Update(const ezWorldModule::UpdateContext& context)
{
  const ezTime tNow = ezTime::Now();

  if (m_pPhysicsInterface == nullptr)
  {
    m_pPhysicsInterface = GetWorld()->GetOrCreateModule<ezPhysicsWorldModuleInterface>();
    return;
  }

  for (auto it = this->m_ComponentStorage.GetIterator(context.m_uiFirstComponentIndex, context.m_uiComponentCount); it.IsValid(); ++it)
  {
    if (it->IsActive())
    {
      //if (tNow - it->m_LastUpdate > ezTime::Seconds(0.1))
      {
        it->m_LastUpdate = tNow;
        it->ScanArea(m_pPhysicsInterface);
      }

      it->Update();
    }
  }
}

//////////////////////////////////////////////////////////////////////////

// clang-format off
EZ_BEGIN_COMPONENT_TYPE(ezDroneSteeringComponent, 1, ezComponentMode::Dynamic)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("MaxForce", m_fSpeedForce)->AddAttributes(new ezDefaultValueAttribute(10.0f), new ezClampValueAttribute(0.0f, ezVariant())),
    EZ_MEMBER_PROPERTY("ScanningDistance", m_fScanningDistance)->AddAttributes(new ezDefaultValueAttribute(15.0f), new ezClampValueAttribute(0.0f, ezVariant())),
    EZ_MEMBER_PROPERTY("ArriveDistance", m_fArriveDistance)->AddAttributes(new ezDefaultValueAttribute(2.0f), new ezClampValueAttribute(0.0f, ezVariant())),
  }
  EZ_END_PROPERTIES;

  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("AI"),
  }
  EZ_END_ATTRIBUTES;
}
EZ_END_COMPONENT_TYPE
// clang-format on

ezDroneSteeringComponent::ezDroneSteeringComponent()
{
  m_vLastFlyDir.SetZero();
}

void ezDroneSteeringComponent::OnSimulationStarted()
{
  m_vCurrentTargetPosition = GetOwner()->GetGlobalPosition();
}

void ezDroneSteeringComponent::SerializeComponent(ezWorldWriter& stream) const
{
  SUPER::SerializeComponent(stream);
  auto& s = stream.GetStream();
}

void ezDroneSteeringComponent::DeserializeComponent(ezWorldReader& stream)
{
  SUPER::DeserializeComponent(stream);
  // const ezUInt32 uiVersion = stream.GetComponentTypeVersion(GetStaticRTTI());
  auto& s = stream.GetStream();
}


void ezDroneSteeringComponent::ScanArea(ezPhysicsWorldModuleInterface* pPhysicsInterface)
{
  const ezWorld* pWorld = GetWorld();
  const ezHeadBoneComponentManager* pManager = pWorld->GetComponentManager<ezHeadBoneComponentManager>();
  ezGameObject* pOwner = GetOwner();

  if (pManager == nullptr)
    return;

  const ezVec3 vCurPos = pOwner->GetGlobalPosition();
  const ezVec3 vFuturePos = vCurPos + pOwner->GetVelocity(); // position in one second

  float fClosestTargetSQR = ezMath::BasicType<float>::MaxValue();
  m_vCurrentTargetPosition = pOwner->GetGlobalPosition();

  for (auto it = pManager->GetComponents(); it.IsValid(); it.Next())
  {
    const ezVec3 vObjPos = it->GetOwner()->GetGlobalPosition();

    const float fDistToObjSQR = (vFuturePos - vObjPos).GetLengthSquared();

    if (fDistToObjSQR < fClosestTargetSQR)
    {
      fClosestTargetSQR = fDistToObjSQR;
      m_vCurrentTargetPosition = vObjPos;
    }
  }

  if (fClosestTargetSQR > ezMath::Square(m_fScanningDistance))
    return;

  m_vLastFlyDir = FindBestDirection(pPhysicsInterface, vCurPos, m_vCurrentTargetPosition);
}

void ezDroneSteeringComponent::Update()
{
  ezGameObject* pOwner = GetOwner();
  const ezVec3 vCurPos = pOwner->GetGlobalPosition();

  if ((m_vCurrentTargetPosition - vCurPos).GetLengthSquared() <= ezMath::Square(m_fArriveDistance))
    return;

  // apply force to self for steering

  ezMsgPhysicsAddForce msg;
  msg.m_vForce = m_vLastFlyDir * m_fSpeedForce;
  msg.m_vGlobalPosition = vCurPos;

  pOwner->SendMessage(msg);
}

ezVec3 ezDroneSteeringComponent::FindBestDirection(
  ezPhysicsWorldModuleInterface* pPhysicsInterface, const ezVec3& vPos, const ezVec3& vTarget) const
{
  ezStaticArray<ezDebugRenderer::Line, 16> linesRed;
  ezStaticArray<ezDebugRenderer::Line, 16> linesGreen;
  ezStaticArray<ezDebugRenderer::Line, 16> linesYellow;

  const ezVec3 vDirToTarget = vTarget - vPos;
  const float fDistanceToTarget = vDirToTarget.GetLength();
  const ezVec3 vDirToTargetNorm = vDirToTarget / fDistanceToTarget;

  ezPhysicsHitResult res;
  if (!pPhysicsInterface->CastRay(vPos, vDirToTargetNorm, fDistanceToTarget, 0, res, ezPhysicsShapeType::Static))
  {
    auto& l = linesGreen.ExpandAndGetRef();
    l.m_start = vPos;
    l.m_end = vPos + vDirToTargetNorm;
    ezDebugRenderer::DrawLines(GetWorld(), linesGreen, ezColor::Lime);

    return vDirToTargetNorm;
  }

  ezVec3 vForwardDir = (vTarget - vPos);
  vForwardDir.z = 0;
  vForwardDir.NormalizeIfNotZero(ezVec3(1, 0, 0));

  const float fCastLength = 5.0f;

  if (!pPhysicsInterface->CastRay(vPos, vForwardDir, fCastLength, 0, res, ezPhysicsShapeType::Static))
  {
    auto& l = linesGreen.ExpandAndGetRef();
    l.m_start = vPos;
    l.m_end = vPos + vForwardDir;
    ezDebugRenderer::DrawLines(GetWorld(), linesGreen, ezColor::Lime);

    return vForwardDir;
  }

  const ezVec3 vUpDir(0, 0, 1);
  const ezVec3 vRightDir = vForwardDir.CrossRH(vUpDir).GetNormalized();

  ezVec3 vSideDirs[4] = {-vUpDir, vUpDir, -vRightDir, vRightDir};
  float fBestPrio = 0;
  ezInt32 iBestPrio = -1;

  for (ezUInt32 i = 0; i < 4; ++i)
  {
    float fSideDistance = fCastLength;

    ezPhysicsHitResult res;
    if (pPhysicsInterface->CastRay(vPos, vSideDirs[i], fCastLength, 0, res, ezPhysicsShapeType::Static))
      fSideDistance = res.m_fDistance;

    auto& l = linesYellow.ExpandAndGetRef();
    l.m_start = vPos;
    l.m_end = vPos + vSideDirs[i] * fSideDistance;

    const float fPrio = ComputeBestLength(pPhysicsInterface, vPos, vSideDirs[i], fSideDistance, vForwardDir);

    if (fPrio > fBestPrio)
    {
      fBestPrio = fPrio;
      iBestPrio = i;
    }
  }

  ezDebugRenderer::DrawLines(GetWorld(), linesYellow, ezColor::Yellow);

  if (iBestPrio < 0)
    return ezVec3::ZeroVector();

  auto& l = linesGreen.ExpandAndGetRef();
  l.m_start = vPos;
  l.m_end = vPos + vSideDirs[iBestPrio];

  ezDebugRenderer::DrawLines(GetWorld(), linesGreen, ezColor::Lime);

  return vSideDirs[iBestPrio];
}

float ezDroneSteeringComponent::ComputeBestLength(ezPhysicsWorldModuleInterface* pPhysicsInterface, const ezVec3& vStart,
  const ezVec3& vSideDir, float fSideDist, const ezVec3& vForwardDir) const
{
  const float fCastLength = 5.0f;

  float fBestLength = -fCastLength;
  for (float f = 1.0f; f < fSideDist; f += 1.0f)
  {
    const ezVec3 start = vStart + vSideDir * f;

    ezPhysicsHitResult res;
    if (pPhysicsInterface->CastRay(start, vForwardDir, fCastLength, 0, res, ezPhysicsShapeType::Static))
    {
      if (res.m_fDistance - f > fBestLength)
        fBestLength = res.m_fDistance - f;
    }
    else
    {
      return fCastLength - f;
    }
  }

  return fBestLength;
}
