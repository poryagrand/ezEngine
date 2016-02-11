#pragma once

#include <RendererCore/Pipeline/Declarations.h>
#include <Core/ResourceManager/ResourceHandle.h>

typedef ezResourceHandle<class ezConstantBufferResource> ezConstantBufferResourceHandle;

/// \brief This renderer is used to gather the information on all visible lights.
class EZ_RENDERERCORE_DLL ezLightGatheringRenderer : public ezRenderer
{
  EZ_ADD_DYNAMIC_REFLECTION(ezLightGatheringRenderer, ezRenderer);

public:
  // ezRenderer implementation
  virtual void GetSupportedRenderDataTypes(ezHybridArray<const ezRTTI*, 8>& types) override;
  virtual ezUInt32 Render(const ezRenderViewContext& renderContext, ezRenderPipelinePass* pPass, const ezArrayPtr<const ezRenderData* const>& renderData) override;

private:
  // TODO: CB Handles
  //ezConstantBufferResourceHandle m_hObjectTransformCB;
};
