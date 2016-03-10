#include <PhysXPlugin/PCH.h>
#include <PhysXPlugin/Resources/PhysXMeshResource.h>
#include <CoreUtils/Assets/AssetFileHeader.h>
#include <Foundation/IO/ChunkStream.h>
#include <PhysXPlugin/PhysXWorldModule.h>

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezPhysXMeshResource, 1, ezRTTIDefaultAllocator<ezPhysXMeshResource>);
EZ_END_DYNAMIC_REFLECTED_TYPE();

ezPhysXMeshResource::ezPhysXMeshResource() : ezResource<ezPhysXMeshResource, ezPhysXMeshResourceDescriptor>(DoUpdate::OnMainThread, 1)
{
  m_pPxTriangleMesh = nullptr;
  m_pPxConvexMesh = nullptr;

  ModifyMemoryUsage().m_uiMemoryCPU = sizeof(ezPhysXMeshResource);
}

ezPhysXMeshResource::~ezPhysXMeshResource()
{
  EZ_ASSERT_DEBUG(m_pPxTriangleMesh == nullptr, "Collision mesh was not unloaded correctly");
  EZ_ASSERT_DEBUG(m_pPxConvexMesh == nullptr, "Collision mesh was not unloaded correctly");
}

ezResourceLoadDesc ezPhysXMeshResource::UnloadData(Unload WhatToUnload)
{
  if (m_pPxTriangleMesh)
  {
    // since it is ref-counted, it may still be in use by the SDK, but we don't bother about that here
    m_pPxTriangleMesh->release();
    m_pPxTriangleMesh = nullptr;
  }

  if (m_pPxConvexMesh)
  {
    // since it is ref-counted, it may still be in use by the SDK, but we don't bother about that here
    m_pPxConvexMesh->release();
    m_pPxConvexMesh = nullptr;
  }

  // we cannot compute this in UpdateMemoryUsage(), so we only read the data there, therefore we need to update this information here
  /// \todo Compute memory usage
  ModifyMemoryUsage().m_uiMemoryCPU = sizeof(ezPhysXMeshResource);

  ezResourceLoadDesc res;
  res.m_uiQualityLevelsDiscardable = 0;
  res.m_uiQualityLevelsLoadable = 0;
  res.m_State = ezResourceState::Unloaded;

  return res;
}

class ezPxInputStream : public PxInputStream
{
public:
  ezPxInputStream(ezStreamReader* pStream) : m_pStream(pStream) {}

  virtual PxU32 read(void* dest, PxU32 count) override
  {
    return (PxU32)m_pStream->ReadBytes(dest, count);
  }

  ezStreamReader* m_pStream;
};

ezResourceLoadDesc ezPhysXMeshResource::UpdateContent(ezStreamReader* Stream)
{
  EZ_LOG_BLOCK("ezPhysXMeshResource::UpdateContent", GetResourceDescription().GetData());

  ezResourceLoadDesc res;
  res.m_uiQualityLevelsDiscardable = 0;
  res.m_uiQualityLevelsLoadable = 0;

  if (Stream == nullptr)
  {
    res.m_State = ezResourceState::LoadedResourceMissing;
    return res;
  }

  // skip the absolute file path data that the standard file reader writes into the stream
  {
    ezString sAbsFilePath;
    (*Stream) >> sAbsFilePath;
  }

  ezAssetFileHeader AssetHash;
  AssetHash.Read(*Stream);

  // load and create the PhysX mesh
  {
    ezChunkStreamReader chunk(*Stream);
    chunk.SetEndChunkFileMode(ezChunkStreamReader::EndChunkFileMode::JustClose);

    chunk.BeginStream();

    // skip all chunks that we don't know
    while (chunk.GetCurrentChunk().m_bValid)
    {
      if (chunk.GetCurrentChunk().m_sChunkName == "TriangleMesh")
      {
        ezPxInputStream PassThroughStream(&chunk);

        m_pPxTriangleMesh = ezPhysX::GetSingleton()->GetPhysXAPI()->createTriangleMesh(PassThroughStream);
      }
      
      if (chunk.GetCurrentChunk().m_sChunkName == "ConvexMesh")
      {
        ezPxInputStream PassThroughStream(&chunk);

        m_pPxConvexMesh = ezPhysX::GetSingleton()->GetPhysXAPI()->createConvexMesh(PassThroughStream);
      }

      chunk.NextChunk();
    }

  if (m_pPxTriangleMesh == nullptr && m_pPxConvexMesh == nullptr)
  {
    ezLog::Error("Could neither find a 'TriangleMesh' chunk nor a 'ConvexMesh' chunk in the PhysXMesh file '%s'", GetResourceID().GetData());
  }


    chunk.EndStream();
  }

  res.m_State = ezResourceState::Loaded;
  return res;
}

void ezPhysXMeshResource::UpdateMemoryUsage(MemoryUsage& out_NewMemoryUsage)
{
  // we cannot compute this data here, so we update it wherever we know the memory usage

  out_NewMemoryUsage.m_uiMemoryCPU = ModifyMemoryUsage().m_uiMemoryCPU;
  out_NewMemoryUsage.m_uiMemoryGPU = 0;
}

ezResourceLoadDesc ezPhysXMeshResource::CreateResource(const ezPhysXMeshResourceDescriptor& descriptor)
{
  EZ_REPORT_FAILURE("This resource type does not support creating data.");

  ezResourceLoadDesc res;
  res.m_uiQualityLevelsDiscardable = 0;
  res.m_uiQualityLevelsLoadable = 0;
  res.m_State = ezResourceState::Loaded;

  return res;
}


