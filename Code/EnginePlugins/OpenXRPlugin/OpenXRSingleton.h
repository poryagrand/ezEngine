#pragma once

#include <Core/Graphics/Camera.h>
#include <Foundation/Configuration/Singleton.h>
#include <GameEngine/Interfaces/VRInterface.h>
#include <OpenXRPlugin/Basics.h>
#include <OpenXRPlugin/OpenXRIncludes.h>
#include <RendererCore/Pipeline/Declarations.h>
#include <RendererCore/Shader/ConstantBufferStorage.h>
#include <RendererFoundation/Descriptors/Descriptors.h>
#include <RendererFoundation/Resources/RenderTargetSetup.h>

struct ezGameApplicationExecutionEvent;
typedef ezTypedResourceHandle<class ezShaderResource> ezShaderResourceHandle;

EZ_DEFINE_AS_POD_TYPE(XrSwapchainImageD3D11KHR);
EZ_DEFINE_AS_POD_TYPE(XrViewConfigurationView);

 class EZ_OPENXRPLUGIN_DLL ezOpenXR : public ezVRInterface
{
  EZ_DECLARE_SINGLETON_OF_INTERFACE(ezOpenXR, ezVRInterface);

public:
  ezOpenXR();

  virtual bool IsHmdPresent() const override;

  virtual ezResult Initialize() override;
  virtual void Deinitialize() override;
  virtual bool IsInitialized() const override;

  virtual const ezHMDInfo& GetHmdInfo() const override;
  virtual void GetDeviceList(ezHybridArray<ezVRDeviceID, 64>& out_Devices) const override;
  virtual ezVRDeviceID GetDeviceIDByType(ezVRDeviceType::Enum type) const override;
  virtual const ezVRDeviceState& GetDeviceState(ezVRDeviceID uiDeviceID) const override;
  virtual ezEvent<const ezVRDeviceEvent&>& DeviceEvents() override;

  virtual ezViewHandle CreateVRView(const ezRenderPipelineResourceHandle& hRenderPipeline, ezCamera* pCamera,
                                    ezGALMSAASampleCount::Enum msaaCount) override;
  virtual ezViewHandle GetVRView() const override;
  virtual bool DestroyVRView() override;
  virtual bool SupportsCompanionView() override;
  virtual bool SetCompanionViewRenderTarget(ezGALTextureHandle hRenderTarget) override;
  virtual ezGALTextureHandle GetCompanionViewRenderTarget() const override;

private:
  struct Swapchain
  {
    XrSwapchain handle = nullptr;
    int64_t format = 0;
    ezUInt32 imageCount = 0;
    XrSwapchainImageBaseHeader* images = nullptr;
    uint32_t imageIndex = 0;
  };
  enum class SwapchainType
  {
    Color,
    Depth,
  };

  XrResult InitSystem();
  void DeinitSystem();
  XrResult InitSession();
  void DeinitSession();
  XrResult InitGraphicsPlugin();
  void DeinitGraphicsPlugin();
  XrResult SelectSwapchainFormat(int64_t& colorFormat, int64_t& depthFormat);
  XrSwapchainImageBaseHeader* CreateSwapchainImages(Swapchain& swapchain, SwapchainType type);
  XrResult InitSwapChain();
  void DeinitSwapChain();


  void GameApplicationEventHandler(const ezGameApplicationExecutionEvent& e);
  void GALDeviceEventHandler(const ezGALDeviceEvent& e);
  void OnBeginRender(ezUInt64);

  void ReadHMDInfo();
  void OnDeviceActivated(ezVRDeviceID uiDeviceID);
  void OnDeviceDeactivated(ezVRDeviceID uiDeviceID);

  void UpdatePoses();
  void UpdateHands();
  void SetStageSpace(ezVRStageSpace::Enum space);
  void SetHMDCamera(ezCamera* pCamera);
  void UpdateCamera();

  static XrPosef ConvertTransform(const ezTransform& tr);
  static XrQuaternionf ConvertOrientation(const ezQuat& q);
  static XrVector3f ConvertPosition(const ezVec3& pos);

  //ezMat4 GetHMDProjectionEye(vr::Hmd_Eye nEye, float fNear, float fFar) const;
  //ezMat4 GetHMDEyePose(vr::Hmd_Eye nEye) const;
  //ezString GetTrackedDeviceString(vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop,
  //                                vr::TrackedPropertyError* peError = nullptr) const;
  static ezQuat ConvertOrientation(const XrQuaternionf& q);
  static ezVec3 ConvertPosition(const XrVector3f& pos);

  static ezMat4 ConvertPoseToMatrix(const XrPosef& pose);
  //static ezVec3 ConvertSteamVRVector(const vr::HmdVector3_t& vector);

private:
  struct FrameState
  {
  };

  // Instance
  XrInstance m_instance = nullptr;

  // System
  uint64_t m_systemId = XR_NULL_SYSTEM_ID;
  bool m_supportsDepth = false;

  // Session
  XrSession m_session = nullptr;
  XrSpace m_sceneSpace = nullptr;
  ezEventSubscriptionID m_executionEventsId = 0;
  ezEventSubscriptionID m_beginRenderEventsId = 0;
  ezEventSubscriptionID m_GALdeviceEventsId = 0;

  // Graphics plugin
  XrEnvironmentBlendMode m_blendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  XrGraphicsBindingD3D11KHR m_xrGraphicsBindingD3D11{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
  XrFormFactor m_formFactor{XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY};
  XrViewConfigurationType m_primaryViewConfigurationType{XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};
  ezHybridArray<XrSwapchainImageD3D11KHR, 3> m_colorSwapChainImagesD3D11;
  ezHybridArray<XrSwapchainImageD3D11KHR, 3> m_depthSwapChainImagesD3D11;

  // Swapchain
  XrViewConfigurationView m_primaryConfigView;
  Swapchain m_colorSwapchain;
  Swapchain m_depthSwapchain;
  
  // Views
  XrView m_views[2];
  XrCompositionLayerProjection m_layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  XrCompositionLayerProjectionView m_projectionLayerViews[2];
  XrCompositionLayerDepthInfoKHR m_depthLayerViews[2];

  // State
  bool m_sessionRunning = false;
  bool m_exitRenderLoop = false;
  bool m_requestRestart = false;
  XrSessionState m_sessionState{XR_SESSION_STATE_UNKNOWN};

  //FrameState m_frameState[2];
  ezInt32 m_updateFrame = 0;
  ezInt32 m_renderFrame = 1;
  XrFrameWaitInfo m_frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
  XrFrameState m_frameState{XR_TYPE_FRAME_STATE};
  XrFrameBeginInfo m_frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};

  // XR interface state
  ezHMDInfo m_Info;
  ezEvent<const ezVRDeviceEvent&> m_DeviceEvents;
  ezVRDeviceState m_DeviceState[3];//Hard-coded for now
  const ezInt8 m_iLeftControllerDeviceID = 1;
  const ezInt8 m_iRightControllerDeviceID = 2;

  ezWorld* m_pWorld = nullptr;
  ezCamera* m_pCameraToSynchronize = nullptr;
  ezEnum<ezVRStageSpace> m_StageSpace;

  ezCamera m_VRCamera;
  ezUInt32 m_uiSettingsModificationCounter = 0;
  ezViewHandle m_hView;
  ezGALRenderTargetSetup m_RenderTargetSetup;
  ezGALTextureCreationDescription m_eyeDesc;
  ezGALTextureHandle m_hColorRT;
  ezGALTextureHandle m_hDepthRT;

  ezGALTextureHandle m_hCompanionRenderTarget;
  ezConstantBufferStorageHandle m_hCompanionConstantBuffer;
  ezShaderResourceHandle m_hCompanionShader;
};

