#include <PhysXPlugin/PCH.h>
#include <PhysXPlugin/Shapes/PxShapeConvexComponent.h>
#include <PhysXPlugin/PhysXSceneModule.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <Core/WorldSerializer/WorldReader.h>

EZ_BEGIN_COMPONENT_TYPE(ezPxShapeConvexComponent, 1);
  EZ_BEGIN_PROPERTIES
    EZ_ACCESSOR_PROPERTY("Collision Mesh", GetMeshFile, SetMeshFile)->AddAttributes(new ezAssetBrowserAttribute("Collision Mesh")),
  EZ_END_PROPERTIES
EZ_END_DYNAMIC_REFLECTED_TYPE();

ezPxShapeConvexComponent::ezPxShapeConvexComponent()
{
}


void ezPxShapeConvexComponent::SerializeComponent(ezWorldWriter& stream) const
{
  SUPER::SerializeComponent(stream);

  auto& s = stream.GetStream();

  /// \todo Serialize resource handles more efficiently
  s << GetMeshFile();
}


void ezPxShapeConvexComponent::DeserializeComponent(ezWorldReader& stream)
{
  SUPER::DeserializeComponent(stream);

  auto& s = stream.GetStream();

  ezStringBuilder sTemp;
  s >> sTemp;
  SetMeshFile(sTemp);
}

void ezPxShapeConvexComponent::SetMeshFile(const char* szFile)
{
  if (!ezStringUtils::IsNullOrEmpty(szFile))
  {
    m_hCollisionMesh = ezResourceManager::LoadResource<ezPhysXMeshResource>(szFile);
  }

  if (m_hCollisionMesh.IsValid())
    ezResourceManager::PreloadResource(m_hCollisionMesh, ezTime::Seconds(5.0));
}


const char* ezPxShapeConvexComponent::GetMeshFile() const
{
  if (!m_hCollisionMesh.IsValid())
    return "";

  ezResourceLock<ezPhysXMeshResource> pMesh(m_hCollisionMesh);
  return pMesh->GetResourceID();
}


void ezPxShapeConvexComponent::AddToActor(PxRigidActor* pActor, const ezTransform& ParentTransform)
{
  if (!m_hCollisionMesh.IsValid())
  {
    ezLog::Warning("ezPxShapeConvexComponent '%s' has no collision mesh set.", GetOwner()->GetName());
    return;
  }

  ezResourceLock<ezPhysXMeshResource> pMesh(m_hCollisionMesh);

  if (!pMesh->GetConvexMesh())
  {
    ezLog::Warning("ezPxShapeConvexComponent '%s' has a collision mesh set that does not contain a convex mesh: '%s' ('%s')", GetOwner()->GetName(), pMesh->GetResourceID().GetData(), pMesh->GetResourceDescription().GetData());
    return;
  }

  ezPhysXSceneModule* pModule = static_cast<ezPhysXSceneModule*>(GetManager()->GetUserData());

  const ezTransform OwnerTransform = GetOwner()->GetGlobalTransform();

  ezTransform LocalTransform;
  LocalTransform.SetLocalTransform(ParentTransform, OwnerTransform);

  ezQuat r;
  r.SetFromMat3(LocalTransform.m_Rotation);
  
  PxTransform t;
  t.p = PxVec3(LocalTransform.m_vPosition.x, LocalTransform.m_vPosition.y, LocalTransform.m_vPosition.z);
  t.q = PxQuat(r.v.x, r.v.y, r.v.z, r.w);

  auto pShape = pActor->createShape(PxConvexMeshGeometry(pMesh->GetConvexMesh()), *GetPxMaterial());
  pShape->setLocalPose(t);

  EZ_ASSERT_DEBUG(pShape != nullptr, "PhysX convex shape creation failed");

  PxFilterData filter;
  filter.word0 = EZ_BIT(m_uiCollisionLayer);
  filter.word1 = ezPhysX::GetSingleton()->GetCollisionFilterConfig().GetFilterMask(m_uiCollisionLayer);
  filter.word2 = 0;
  filter.word3 = 0;
  pShape->setSimulationFilterData(filter);
  pShape->setQueryFilterData(filter);

  pShape->userData = GetOwner();
}
