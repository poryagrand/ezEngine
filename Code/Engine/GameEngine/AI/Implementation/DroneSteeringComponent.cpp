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
      if (tNow - it->m_LastUpdate > ezTime::Seconds(0.1))
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
  ezVec3 vBestTarget(0);

  for (auto it = pManager->GetComponents(); it.IsValid(); it.Next())
  {
    const ezVec3 vObjPos = it->GetOwner()->GetGlobalPosition();

    const float fDistToObjSQR = (vFuturePos - vObjPos).GetLengthSquared();

    if (fDistToObjSQR < fClosestTargetSQR)
    {
      fClosestTargetSQR = fDistToObjSQR;
      vBestTarget = vObjPos;
    }
  }

  if (fClosestTargetSQR > ezMath::Square(m_fScanningDistance))
    return;

  m_vLastFlyDir = FindBestDirection(pPhysicsInterface, vCurPos, vBestTarget - vFuturePos);
}

void ezDroneSteeringComponent::Update()
{
  if (m_vLastFlyDir.GetLength() <= m_fArriveDistance)
    return;

  ezGameObject* pOwner = GetOwner();
  const ezVec3 vCurPos = pOwner->GetGlobalPosition();

  // apply force to self for steering

  ezMsgPhysicsAddForce msg;
  msg.m_vForce = m_vLastFlyDir * m_fSpeedForce;
  msg.m_vGlobalPosition = vCurPos;

  pOwner->SendMessage(msg);
}

ezVec3 ezDroneSteeringComponent::FindBestDirection(
  ezPhysicsWorldModuleInterface* pPhysicsInterface, const ezVec3& vPos, const ezVec3& vForward) const
{
  const float fOrgLen = vForward.GetLength();
  const float fCastLen = ezMath::Min(fOrgLen, 10.0f);

  const ezVec3 vDir = vForward / fOrgLen;

  struct Whisker
  {
    ezQuat m_Rotation;
    ezVec3 m_vWhiskerDir;
  };

  ezStaticArray<ezDebugRenderer::Line, 16> linesRed;
  ezStaticArray<ezDebugRenderer::Line, 16> linesGreen;
  ezStaticArray<ezDebugRenderer::Line, 16> linesYellow;

  ezStaticArray<Whisker, 16> whiskers;

  whiskers.ExpandAndGetRef().m_Rotation.SetIdentity();

  whiskers.ExpandAndGetRef().m_Rotation.SetFromAxisAndAngle(ezVec3(0, 0, 1), ezAngle::Degree(+10));
  whiskers.ExpandAndGetRef().m_Rotation.SetFromAxisAndAngle(ezVec3(0, 0, 1), ezAngle::Degree(-10));

  whiskers.ExpandAndGetRef().m_Rotation.SetFromAxisAndAngle(ezVec3(0, 1, 0), ezAngle::Degree(+10));
  whiskers.ExpandAndGetRef().m_Rotation.SetFromAxisAndAngle(ezVec3(0, 1, 0), ezAngle::Degree(-10));

  whiskers.ExpandAndGetRef().m_Rotation.SetFromAxisAndAngle(ezVec3(0, 0, 1), ezAngle::Degree(+25));
  whiskers.ExpandAndGetRef().m_Rotation.SetFromAxisAndAngle(ezVec3(0, 0, 1), ezAngle::Degree(-25));

  whiskers.ExpandAndGetRef().m_Rotation.SetFromAxisAndAngle(ezVec3(0, 0, 1), ezAngle::Degree(+45));
  whiskers.ExpandAndGetRef().m_Rotation.SetFromAxisAndAngle(ezVec3(0, 0, 1), ezAngle::Degree(-45));

  whiskers.ExpandAndGetRef().m_Rotation.SetFromAxisAndAngle(ezVec3(0, 1, 0), ezAngle::Degree(+25));
  whiskers.ExpandAndGetRef().m_Rotation.SetFromAxisAndAngle(ezVec3(0, 1, 0), ezAngle::Degree(-25));

  whiskers.ExpandAndGetRef().m_Rotation.SetFromAxisAndAngle(ezVec3(0, 1, 0), ezAngle::Degree(+45));
  whiskers.ExpandAndGetRef().m_Rotation.SetFromAxisAndAngle(ezVec3(0, 1, 0), ezAngle::Degree(-45));

  ezUInt32 uiBestWhisker = 0xFFFFFFFF;
  float fBestWhiskerLength = 0.0f;

  for (ezUInt32 i = 0; i < whiskers.GetCount(); ++i)
  {
    auto& w = whiskers[i];

    w.m_vWhiskerDir = w.m_Rotation * vDir;

    ezPhysicsHitResult res;
    if (!pPhysicsInterface->CastRay(vPos, w.m_vWhiskerDir, fCastLen, 0, res, ezPhysicsShapeType::Static))
    {
      if (fCastLen > fBestWhiskerLength)
      {
        uiBestWhisker = i;
        fBestWhiskerLength = fCastLen * 2.0f;
      }

      if (!cvar_AiVisSteering)
        break;

      auto& l = linesYellow.ExpandAndGetRef();
      l.m_start = vPos;
      l.m_end = vPos + w.m_vWhiskerDir * fCastLen;
    }
    else
    {
      auto& l = linesRed.ExpandAndGetRef();
      l.m_start = vPos;
      l.m_end = res.m_vPosition;

      if (res.m_fDistance > fBestWhiskerLength)
      {
        uiBestWhisker = i;
        fBestWhiskerLength = res.m_fDistance;
      }
    }
  }

  if (uiBestWhisker == 0xFFFFFFFF)
  {
    uiBestWhisker = 0;

    auto& l = linesYellow.ExpandAndGetRef();
    l.m_start = vPos;
    l.m_end = vPos + whiskers[uiBestWhisker].m_vWhiskerDir * fCastLen;
  }
  else
  {
    auto& l = linesGreen.ExpandAndGetRef();
    l.m_start = vPos;
    l.m_end = vPos + whiskers[uiBestWhisker].m_vWhiskerDir * fCastLen;
  }

  if (cvar_AiVisSteering)
  {
    ezDebugRenderer::DrawLines(GetWorld(), linesGreen, ezColor::Lime);
    ezDebugRenderer::DrawLines(GetWorld(), linesYellow, ezColor::Yellow);
    ezDebugRenderer::DrawLines(GetWorld(), linesRed, ezColor::Red);
  }

  return whiskers[uiBestWhisker].m_vWhiskerDir * fOrgLen;
}
