#pragma once

#include <GameEngine/Configuration/PlatformProfile.h>

class EZ_GAMEENGINE_DLL ezVRConfig : public ezProfileConfigData
{
  EZ_ADD_DYNAMIC_REFLECTION(ezVRConfig, ezProfileConfigData);

public:
  virtual void SaveRuntimeData(ezChunkStreamWriter& stream) const override;
  virtual void LoadRuntimeData(ezChunkStreamReader& stream) override;

  bool m_bEnableVR = false;
  ezString m_sVRRenderPipeline;
};

