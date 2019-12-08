#include <GameEnginePCH.h>

#include <GameEngine/Configuration/VRConfig.h>
#include <Foundation/IO/ChunkStream.h>

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezVRConfig, 1, ezRTTIDefaultAllocator<ezVRConfig>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("EnableVR", m_bEnableVR)->AddAttributes(new ezDefaultValueAttribute(false)),
    // HololensRenderPipeline.ezRenderPipelineAsset
    EZ_MEMBER_PROPERTY("VRRenderPipeline", m_sVRRenderPipeline)->AddAttributes(new ezAssetBrowserAttribute("RenderPipeline"), new ezDefaultValueAttribute(ezStringView("{ 2fe25ded-776c-7f9e-354f-e4c52a33d125 }"))),
  }
  EZ_END_PROPERTIES;
}
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

void ezVRConfig::SaveRuntimeData(ezChunkStreamWriter& stream) const
{
  stream.BeginChunk("ezVRConfig", 1);

  stream << m_bEnableVR;
  stream << m_sVRRenderPipeline;

  stream.EndChunk();
}

void ezVRConfig::LoadRuntimeData(ezChunkStreamReader& stream)
{
  const auto& chunk = stream.GetCurrentChunk();

  if (chunk.m_sChunkName == "ezVRConfig" && chunk.m_uiChunkVersion == 1)
  {
    stream >> m_bEnableVR;
    stream >> m_sVRRenderPipeline;
  }
}

EZ_STATICLINK_FILE(GameEngine, GameEngine_Configuration_Implementation_VRConfigs);

