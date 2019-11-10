#include <OpenXRPluginPCH.h>

#include <Foundation/Configuration/CVar.h>
#include <Foundation/Time/Stopwatch.h>
#include <GameEngine/GameApplication/GameApplication.h>
#include <GameEngine/Interfaces/SoundInterface.h>
#include <OpenXRPlugin/OpenXRIncludes.h>
#include <OpenXRPlugin/OpenXRSingleton.h>
#include <RendererCore/GPUResourcePool/GPUResourcePool.h>
#include <RendererCore/Pipeline/View.h>
#include <RendererCore/RenderContext/RenderContext.h>
#include <RendererCore/RenderWorld/RenderWorld.h>
#include <RendererDX11/Resources/TextureDX11.h>
#include <RendererFoundation/Context/Context.h>
#include <RendererFoundation/Device/Device.h>
#include <RendererFoundation/Profiling/Profiling.h>
#include <RendererFoundation/Resources/RenderTargetSetup.h>

#include <../../../Data/Base/Shaders/Pipeline/VRCompanionViewConstants.h>
#include <Core/World/World.h>
#include <GameEngine/VirtualReality/StageSpaceComponent.h>
#include <RendererCore/Shader/ShaderResource.h>
#include <algorithm>
#include <vector>

#if EZ_ENABLED(EZ_PLATFORM_WINDOWS)
#  include <RendererDX11/Context/ContextDX11.h>
#  include <RendererDX11/Device/DeviceDX11.h>
#endif

EZ_IMPLEMENT_SINGLETON(ezOpenXR);

static ezOpenXR g_OpenXRSingleton;

#define XR_CHECK_LOG(exp)                                        \
  {                                                              \
    if (exp != XR_SUCCESS)                                       \
    {                                                            \
      ezLog::Error("OpenXR failure: %s:%d", __FILE__, __LINE__); \
      return;                                                    \
    }                                                            \
  }

#define EZ_CHECK_XR(exp)                                         \
  {                                                              \
    if (exp != XR_SUCCESS)                                       \
    {                                                            \
      ezLog::Error("OpenXR failure: %s:%d", __FILE__, __LINE__); \
      return EZ_FAILURE;                                         \
    }                                                            \
  }

#define XR_SUCCEED_OR_RETURN(code, cleanup) \
  do                                        \
  {                                         \
    auto s = (code);                        \
    if (s != XR_SUCCESS)                    \
    {                                       \
      cleanup();                            \
      return s;                             \
    }                                       \
  } while (false)

#define XR_SUCCEED_OR_RETURN_LOG(code, cleanup)                                  \
  do                                                                             \
  {                                                                              \
    auto s = (code);                                                             \
    if (s != XR_SUCCESS)                                                         \
    {                                                                            \
      ezLog::Error("OpenXR call '{0}' failed with: {1}", EZ_STRINGIZE(code), s); \
      cleanup();                                                                 \
      return s;                                                                  \
    }                                                                            \
  } while (false)

static void voidFunction()
{
}

ezOpenXR::ezOpenXR()
  : m_SingletonRegistrar(this)
{
  // m_DeviceState[0].m_mPose.SetIdentity();
}

bool ezOpenXR::IsHmdPresent() const
{
  //#TODO
  return false;
}

static ezResult SelectExtensions(ezHybridArray<const char*, 6>& extensions)
{
  // Fetch the list of extensions supported by the runtime.
  ezUInt32 extensionCount;
  EZ_CHECK_XR(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr));
  std::vector<XrExtensionProperties> extensionProperties(extensionCount, {XR_TYPE_EXTENSION_PROPERTIES});

  EZ_CHECK_XR(xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensionProperties.data()));

  // Add a specific extension to the list of extensions to be enabled, if it is supported.
  auto AddExtIfSupported = [&](const char* extensionName) -> ezResult {
    auto it = std::find_if(begin(extensionProperties), end(extensionProperties), [&](const XrExtensionProperties& prop) { return ezStringUtils::IsEqual(prop.extensionName, extensionName); });
    if (it != end(extensionProperties))
    {
      extensions.PushBack(extensionName);
      return EZ_SUCCESS;
    }
    return EZ_FAILURE;
  };

  // D3D11 extension is required so check that it was added.
  EZ_SUCCEED_OR_RETURN(AddExtIfSupported(XR_KHR_D3D11_ENABLE_EXTENSION_NAME));

  // Additional optional extensions for enhanced functionality. Track whether enabled in m_optionalExtensions.
  //m_optionalExtensions.UnboundedRefSpaceSupported = AddExtIfSupported(XR_MSFT_UNBOUNDED_REFERENCE_SPACE_EXTENSION_NAME);
  //m_optionalExtensions.SpatialAnchorSupported = AddExtIfSupported(XR_MSFT_SPATIAL_ANCHOR_EXTENSION_NAME);

  return EZ_SUCCESS;
}

ezResult ezOpenXR::Initialize()
{
  if (m_instance != nullptr)
    return EZ_FAILURE;

  // Build out the extensions to enable. Some extensions are required and some are optional.
  ezHybridArray<const char*, 6> enabledExtensions;
  EZ_SUCCEED_OR_RETURN(SelectExtensions(enabledExtensions));

  // Create the instance with desired extensions.
  XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
  createInfo.enabledExtensionCount = (uint32_t)enabledExtensions.GetCount();
  createInfo.enabledExtensionNames = enabledExtensions.GetData();

  ezStringUtils::Copy(createInfo.applicationInfo.applicationName, EZ_ARRAY_SIZE(createInfo.applicationInfo.applicationName), ezApplication::GetApplicationInstance()->GetApplicationName());
  createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
  createInfo.applicationInfo.applicationVersion = 1;
  EZ_CHECK_XR(xrCreateInstance(&createInfo, &m_instance));

  ezLog::Success("OpenXR initialized successfully.");
  return EZ_SUCCESS;
}

void ezOpenXR::Deinitialize()
{
  DeinitSession();
  DeinitSystem();

  if (m_instance)
  {
    xrDestroyInstance(m_instance);
    m_instance = nullptr;
  }
}

bool ezOpenXR::IsInitialized() const
{
  return m_instance != nullptr;
}

const ezHMDInfo& ezOpenXR::GetHmdInfo() const
{
  EZ_ASSERT_DEV(IsInitialized(), "Need to call 'Initialize' first.");
  return m_Info;
}

void ezOpenXR::GetDeviceList(ezHybridArray<ezVRDeviceID, 64>& out_Devices) const
{
  EZ_ASSERT_DEV(IsInitialized(), "Need to call 'Initialize' first.");
  out_Devices.PushBack(0);
  //TODO
  /*for (ezVRDeviceID i = 0; i < vr::k_unMaxTrackedDeviceCount; i++)
  {
    if (m_DeviceState[i].m_bDeviceIsConnected)
    {
      out_Devices.PushBack(i);
    }
  }*/
}

ezVRDeviceID ezOpenXR::GetDeviceIDByType(ezVRDeviceType::Enum type) const
{
  ezVRDeviceID deviceID = -1;
  switch (type)
  {
    case ezVRDeviceType::HMD:
      deviceID = 0;
      break;
    case ezVRDeviceType::LeftController:
      deviceID = m_iLeftControllerDeviceID;
      break;
    case ezVRDeviceType::RightController:
      deviceID = m_iRightControllerDeviceID;
      break;
    default:
      deviceID = -1;
      break;
  }

  if (deviceID != -1 && !m_DeviceState[deviceID].m_bDeviceIsConnected)
  {
    deviceID = -1;
  }
  return deviceID;
}

const ezVRDeviceState& ezOpenXR::GetDeviceState(ezVRDeviceID uiDeviceID) const
{
  EZ_ASSERT_DEV(IsInitialized(), "Need to call 'Initialize' first.");
  EZ_ASSERT_DEV(uiDeviceID < 3, "Invalid device ID.");
  EZ_ASSERT_DEV(m_DeviceState[uiDeviceID].m_bDeviceIsConnected, "Invalid device ID.");
  return m_DeviceState[uiDeviceID];
}

ezEvent<const ezVRDeviceEvent&>& ezOpenXR::DeviceEvents()
{
  return m_DeviceEvents;
}

ezViewHandle ezOpenXR::CreateVRView(
  const ezRenderPipelineResourceHandle& hRenderPipeline, ezCamera* pCamera, ezGALMSAASampleCount::Enum msaaCount)
{
  EZ_ASSERT_DEV(IsInitialized(), "Need to call 'Initialize' first.");

  XrResult res = InitSystem();
  if (res != XrResult::XR_SUCCESS)
  {
    ezLog::Error("Bla");
    return ezViewHandle();
  }
  res = InitSession();
  if (res != XrResult::XR_SUCCESS)
  {
    DeinitSystem();
    ezLog::Error("Bla");
    return ezViewHandle();
  }

  SetHMDCamera(pCamera);

  ezGALDevice* pDevice = ezGALDevice::GetDefaultDevice();

  ezView* pMainView = nullptr;
  m_hView = ezRenderWorld::CreateView("Holographic View", pMainView);
  pMainView->SetCameraUsageHint(ezCameraUsageHint::MainView);

  {
    ezGALTextureCreationDescription tcd;
    tcd.SetAsRenderTarget(
      m_Info.m_vEyeRenderTargetSize.x, m_Info.m_vEyeRenderTargetSize.y, ezGALResourceFormat::RGBAUByteNormalizedsRGB, msaaCount);
    tcd.m_uiArraySize = 2;
    m_hColorRT = pDevice->CreateTexture(tcd);

    // Store desc for one eye for later.
    m_eyeDesc = tcd;
    m_eyeDesc.m_uiArraySize = 1;
  }
  {
    ezGALTextureCreationDescription tcd;
    tcd.SetAsRenderTarget(m_Info.m_vEyeRenderTargetSize.x, m_Info.m_vEyeRenderTargetSize.y, ezGALResourceFormat::DFloat, msaaCount);
    tcd.m_uiArraySize = 2;
    m_hDepthRT = pDevice->CreateTexture(tcd);
  }

  m_RenderTargetSetup.SetRenderTarget(0, pDevice->GetDefaultRenderTargetView(m_hColorRT))
    .SetDepthStencilTarget(pDevice->GetDefaultRenderTargetView(m_hDepthRT));

  pMainView->SetRenderTargetSetup(m_RenderTargetSetup);
  pMainView->SetRenderPipelineResource(hRenderPipeline);
  pMainView->SetCamera(&m_VRCamera);
  pMainView->SetRenderPassProperty("ColorSource", "MSAA_Mode", (ezInt32)msaaCount);
  pMainView->SetRenderPassProperty("DepthStencil", "MSAA_Mode", (ezInt32)msaaCount);

  pMainView->SetViewport(ezRectFloat((float)m_Info.m_vEyeRenderTargetSize.x, (float)m_Info.m_vEyeRenderTargetSize.y));

  ezRenderWorld::AddMainView(m_hView);
  return m_hView;
}

ezViewHandle ezOpenXR::GetVRView() const
{
  return m_hView;
}

bool ezOpenXR::DestroyVRView()
{
  if (m_hView.IsInvalidated())
    return false;

  m_pWorld = nullptr;
  SetHMDCamera(nullptr);

  //#TODO vr::VRCompositor()->ClearLastSubmittedFrame();
  ezGALDevice* pDevice = ezGALDevice::GetDefaultDevice();

  ezRenderWorld::RemoveMainView(m_hView);
  ezRenderWorld::DeleteView(m_hView);
  m_hView.Invalidate();
  m_RenderTargetSetup.DestroyAllAttachedViews();

  pDevice->DestroyTexture(m_hColorRT);
  m_hColorRT.Invalidate();
  pDevice->DestroyTexture(m_hDepthRT);
  m_hDepthRT.Invalidate();

  DeinitSession();
  DeinitSystem();
  return true;
}

bool ezOpenXR::SupportsCompanionView()
{
  return true;
}

bool ezOpenXR::SetCompanionViewRenderTarget(ezGALTextureHandle hRenderTarget)
{
  if (!m_hCompanionRenderTarget.IsInvalidated() && !hRenderTarget.IsInvalidated())
  {
    // Maintain already created resources (just switch target).
  }
  else if (!m_hCompanionRenderTarget.IsInvalidated() && hRenderTarget.IsInvalidated())
  {
    // Delete companion resources.
    ezRenderContext::DeleteConstantBufferStorage(m_hCompanionConstantBuffer);
    m_hCompanionConstantBuffer.Invalidate();
  }
  else if (m_hCompanionRenderTarget.IsInvalidated() && !hRenderTarget.IsInvalidated())
  {
    // Create companion resources.
    m_hCompanionShader = ezResourceManager::LoadResource<ezShaderResource>("Shaders/Pipeline/VRCompanionView.ezShader");
    EZ_ASSERT_DEV(m_hCompanionShader.IsValid(), "Could not load VR companion view shader!");
    m_hCompanionConstantBuffer = ezRenderContext::CreateConstantBufferStorage<ezVRCompanionViewConstants>();
    m_hCompanionRenderTarget = hRenderTarget;
  }
  return true;
}

ezGALTextureHandle ezOpenXR::GetCompanionViewRenderTarget() const
{
  return m_hCompanionRenderTarget;
}

XrResult ezOpenXR::InitSystem()
{
  EZ_ASSERT_DEV(m_systemId == XR_NULL_SYSTEM_ID, "");
  XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
  systemInfo.formFactor = XrFormFactor::XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  XR_SUCCEED_OR_RETURN_LOG(xrGetSystem(m_instance, &systemInfo, &m_systemId), DeinitSystem);
  return XrResult::XR_SUCCESS;
}

void ezOpenXR::DeinitSystem()
{
  m_systemId = XR_NULL_SYSTEM_ID;
}

XrResult ezOpenXR::InitSession()
{
  EZ_ASSERT_DEV(m_session == nullptr, "");
  XR_SUCCEED_OR_RETURN(InitGraphicsPlugin(), DeinitSession);

  XrSessionCreateInfo sessionCreateInfo{XR_TYPE_SESSION_CREATE_INFO};
  sessionCreateInfo.next = &m_xrGraphicsBindingD3D11;
  sessionCreateInfo.systemId = m_systemId;
  XR_SUCCEED_OR_RETURN_LOG(xrCreateSession(m_instance, &sessionCreateInfo, &m_session), DeinitSession);

  XrReferenceSpaceCreateInfo spaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
  spaceCreateInfo.poseInReferenceSpace = ConvertTransform(ezTransform::IdentityTransform());
  XR_SUCCEED_OR_RETURN_LOG(xrCreateReferenceSpace(m_session, &spaceCreateInfo, &m_sceneSpace), DeinitSession);

  XR_SUCCEED_OR_RETURN(InitSwapChain(), DeinitSession);

  m_executionEventsId = ezGameApplicationBase::GetGameApplicationBaseInstance()->m_ExecutionEvents.AddEventHandler(
    ezMakeDelegate(&ezOpenXR::GameApplicationEventHandler, this));
  m_beginRenderEventsId = ezRenderWorld::s_BeginRenderEvent.AddEventHandler(ezMakeDelegate(&ezOpenXR::OnBeginRender, this));
  m_GALdeviceEventsId = ezGALDevice::GetDefaultDevice()->m_Events.AddEventHandler(ezMakeDelegate(&ezOpenXR::GALDeviceEventHandler, this));

  ReadHMDInfo();
  SetStageSpace(ezVRStageSpace::Standing);
  UpdatePoses();

  return XrResult::XR_SUCCESS;
}

void ezOpenXR::DeinitSession()
{
  DeinitSwapChain();

  if (m_executionEventsId != 0)
  {
    ezGameApplicationBase::GetGameApplicationBaseInstance()->m_ExecutionEvents.RemoveEventHandler(m_executionEventsId);
    ezRenderWorld::s_BeginRenderEvent.RemoveEventHandler(m_beginRenderEventsId);
    ezGALDevice::GetDefaultDevice()->m_Events.RemoveEventHandler(m_GALdeviceEventsId);
  }

  if (m_sceneSpace)
  {
    xrDestroySpace(m_sceneSpace);
    m_sceneSpace = nullptr;
  }

  if (m_session)
  {
    xrDestroySession(m_session);
    m_session = nullptr;
  }

  DeinitGraphicsPlugin();


  SetCompanionViewRenderTarget(ezGALTextureHandle());
  DestroyVRView();
}

XrResult ezOpenXR::InitGraphicsPlugin()
{
  EZ_ASSERT_DEV(m_xrGraphicsBindingD3D11.device == nullptr, "");
  // Hard-coded to d3d
  XrGraphicsRequirementsD3D11KHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
  XR_SUCCEED_OR_RETURN_LOG(xrGetD3D11GraphicsRequirementsKHR(m_instance, m_systemId, &graphicsRequirements), DeinitGraphicsPlugin);
  ezGALDevice* pDevice = ezGALDevice::GetDefaultDevice();
  ezGALDeviceDX11* pD3dDevice = static_cast<ezGALDeviceDX11*>(pDevice);

  m_xrGraphicsBindingD3D11.device = pD3dDevice->GetDXDevice();

  return XrResult::XR_SUCCESS;
}

void ezOpenXR::DeinitGraphicsPlugin()
{
  m_xrGraphicsBindingD3D11.device = nullptr;
}

XrResult ezOpenXR::SelectSwapchainFormat(int64_t& colorFormat, int64_t& depthFormat)
{
  uint32_t swapchainFormatCount;
  XR_SUCCEED_OR_RETURN_LOG(xrEnumerateSwapchainFormats(m_session, 0, &swapchainFormatCount, nullptr), voidFunction);
  std::vector<int64_t> swapchainFormats(swapchainFormatCount);
  XR_SUCCEED_OR_RETURN_LOG(xrEnumerateSwapchainFormats(
                             m_session, (uint32_t)swapchainFormats.size(), &swapchainFormatCount, swapchainFormats.data()),
    voidFunction);

  // List of supported color swapchain formats, in priority order.
  constexpr DXGI_FORMAT SupportedColorSwapchainFormats[] = {
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
  };

  constexpr DXGI_FORMAT SupportedDepthSwapchainFormats[] = {
    DXGI_FORMAT_D32_FLOAT,
    DXGI_FORMAT_D16_UNORM,
    DXGI_FORMAT_D24_UNORM_S8_UINT,
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
  };

  auto swapchainFormatIt = std::find_first_of(std::begin(SupportedColorSwapchainFormats),
    std::end(SupportedColorSwapchainFormats),
    swapchainFormats.begin(),
    swapchainFormats.end());
  if (swapchainFormatIt == std::end(SupportedColorSwapchainFormats))
  {
    return XrResult::XR_ERROR_INITIALIZATION_FAILED;
  }
  colorFormat = *swapchainFormatIt;

  if (m_supportsDepth)
  {
    auto depthSwapchainFormatIt = std::find_first_of(std::begin(SupportedDepthSwapchainFormats),
      std::end(SupportedDepthSwapchainFormats),
      swapchainFormats.begin(),
      swapchainFormats.end());
    if (depthSwapchainFormatIt == std::end(SupportedDepthSwapchainFormats))
    {
      return XrResult::XR_ERROR_INITIALIZATION_FAILED;
    }
    depthFormat = *depthSwapchainFormatIt;
  }
  return XrResult::XR_SUCCESS;
}

XrSwapchainImageBaseHeader* ezOpenXR::CreateSwapchainImages(Swapchain& swapchain, SwapchainType type)
{
  if (type == SwapchainType::Color)
  {
    m_colorSwapChainImagesD3D11.SetCount(swapchain.imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
    return reinterpret_cast<XrSwapchainImageBaseHeader*>(m_colorSwapChainImagesD3D11.GetData());
  }
  else
  {
    m_depthSwapChainImagesD3D11.SetCount(swapchain.imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
    return reinterpret_cast<XrSwapchainImageBaseHeader*>(m_depthSwapChainImagesD3D11.GetData());
  }
}

XrResult ezOpenXR::InitSwapChain()
{
  // Read graphics properties for preferred swapchain length and logging.
  XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
  XR_SUCCEED_OR_RETURN_LOG(xrGetSystemProperties(m_instance, m_systemId, &systemProperties), DeinitSwapChain);

  ezUInt32 viewCount = 0;
  XR_SUCCEED_OR_RETURN_LOG(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_primaryViewConfigurationType, 0, &viewCount, nullptr), DeinitSwapChain);
  if (viewCount != 2)
  {
    ezLog::Error("No stereo view configuration present, can't create swap chain");
    DeinitSwapChain();
    return XR_ERROR_INITIALIZATION_FAILED;
  }
  ezHybridArray<XrViewConfigurationView, 2> views;
  views.SetCount(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
  XR_SUCCEED_OR_RETURN_LOG(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_primaryViewConfigurationType, viewCount, &viewCount, views.GetData()), DeinitSwapChain);

  // Create the swapchain and get the images.
  // Select a swapchain format.
  m_primaryConfigView = views[0];
  XR_SUCCEED_OR_RETURN_LOG(SelectSwapchainFormat(m_colorSwapchain.format, m_depthSwapchain.format), DeinitSwapChain);

  // Create the swapchain.
  XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
  swapchainCreateInfo.arraySize = 2;
  swapchainCreateInfo.format = m_colorSwapchain.format;
  swapchainCreateInfo.width = m_primaryConfigView.recommendedImageRectWidth;
  swapchainCreateInfo.height = m_primaryConfigView.recommendedImageRectHeight;
  swapchainCreateInfo.mipCount = 1;
  swapchainCreateInfo.faceCount = 1;
  swapchainCreateInfo.sampleCount = 0;
  swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

  m_Info.m_vEyeRenderTargetSize.Set(swapchainCreateInfo.width, swapchainCreateInfo.height);

  auto CreateSwapChain = [this](const XrSwapchainCreateInfo& swapchainCreateInfo, Swapchain& swapchain, SwapchainType type) -> XrResult {
    XR_SUCCEED_OR_RETURN_LOG(xrCreateSwapchain(m_session, &swapchainCreateInfo, &swapchain.handle), voidFunction);
    XR_SUCCEED_OR_RETURN_LOG(xrEnumerateSwapchainImages(swapchain.handle, 0, &swapchain.imageCount, nullptr), voidFunction);
    swapchain.images = CreateSwapchainImages(swapchain, type);
    XR_SUCCEED_OR_RETURN_LOG(xrEnumerateSwapchainImages(swapchain.handle, swapchain.imageCount, &swapchain.imageCount, swapchain.images), voidFunction);
    return XrResult::XR_SUCCESS;
  };
  XR_SUCCEED_OR_RETURN(CreateSwapChain(swapchainCreateInfo, m_colorSwapchain, SwapchainType::Color), DeinitSwapChain);

  if (m_supportsDepth)
  {
    swapchainCreateInfo.format = m_depthSwapchain.format;
    swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    XR_SUCCEED_OR_RETURN(CreateSwapChain(swapchainCreateInfo, m_depthSwapchain, SwapchainType::Depth), DeinitSwapChain);
  }
  return XrResult::XR_SUCCESS;
}

void ezOpenXR::DeinitSwapChain()
{
  auto DeleteSwapchain = [](Swapchain& swapchain) {
    if (swapchain.handle != nullptr)
    {
      xrDestroySwapchain(swapchain.handle);
      swapchain.handle = nullptr;
    }
    swapchain.format = 0;
    swapchain.imageCount = 0;
    swapchain.images = nullptr;
    swapchain.imageIndex = 0;
  };
  m_primaryConfigView = {XR_TYPE_VIEW_CONFIGURATION_VIEW};
  DeleteSwapchain(m_colorSwapchain);
  DeleteSwapchain(m_depthSwapchain);

  m_colorSwapChainImagesD3D11.Clear();
  m_depthSwapChainImagesD3D11.Clear();
}

void ezOpenXR::GALDeviceEventHandler(const ezGALDeviceEvent& e)
{
  if (e.m_Type == ezGALDeviceEvent::Type::BeforeBeginFrame)
  {
    m_frameWaitInfo = XrFrameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
    m_frameState = XrFrameState{XR_TYPE_FRAME_STATE};
    XrResult result = xrWaitFrame(m_session, &m_frameWaitInfo, &m_frameState);
    if (result != XR_SUCCESS)
    {
      //TODO?
    }
    m_frameBeginInfo = XrFrameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    result = xrBeginFrame(m_session, &m_frameBeginInfo);
    if (result != XR_SUCCESS)
    {
      //TODO?
    }
    UpdatePoses();

    auto AquireAndWait = [](Swapchain& swapchain) {
      XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
      XR_CHECK_LOG(xrAcquireSwapchainImage(swapchain.handle, &acquireInfo, &swapchain.imageIndex));

      XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
      waitInfo.timeout = XR_INFINITE_DURATION;
      XR_CHECK_LOG(xrWaitSwapchainImage(swapchain.handle, &waitInfo));
    };
    AquireAndWait(m_colorSwapchain);
    if (m_supportsDepth)
      AquireAndWait(m_depthSwapchain);

    // This will update the extracted view from last frame with the new data we got
    // this frame just before starting to render.
    ezView* pView = nullptr;
    if (ezRenderWorld::TryGetView(m_hView, pView))
    {
      pView->UpdateViewData(ezRenderWorld::GetDataIndexForRendering());
    }
  }
}

void ezOpenXR::GameApplicationEventHandler(const ezGameApplicationExecutionEvent& e)
{
  EZ_ASSERT_DEV(IsInitialized(), "Need to call 'Initialize' first.");

  if (e.m_Type == ezGameApplicationExecutionEvent::Type::BeforeUpdatePlugins)
  {
    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER, nullptr};
    XrEventDataBaseHeader* header = reinterpret_cast<XrEventDataBaseHeader*>(&event);

    while (xrPollEvent(m_instance, &event) == XR_SUCCESS)
    {
      switch (event.type)
      {
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
        {
          const XrEventDataSessionStateChanged& session_state_changed_event =
            *reinterpret_cast<XrEventDataSessionStateChanged*>(&event);
          m_sessionState = session_state_changed_event.state;
          switch (m_sessionState)
          {
            case XR_SESSION_STATE_READY:
            {
              XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
              sessionBeginInfo.primaryViewConfigurationType = m_primaryViewConfigurationType;
              if (xrBeginSession(m_session, &sessionBeginInfo) == XR_SUCCESS)
              {
                m_sessionRunning = true;
              }
              break;
            }
            case XR_SESSION_STATE_STOPPING:
            {
              m_sessionRunning = false;
              if (xrEndSession(m_session) != XR_SUCCESS)
              {
                //TODO log
              }
              break;
            }
            case XR_SESSION_STATE_EXITING:
            {
              // Do not attempt to restart because user closed this session.
              m_exitRenderLoop = true;
              m_requestRestart = false;
              break;
            }
            case XR_SESSION_STATE_LOSS_PENDING:
            {
              // Poll for a new systemId
              m_exitRenderLoop = true;
              m_requestRestart = true;
              break;
            }
          }
          break;
        }
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
        {
          const XrEventDataInstanceLossPending& instance_loss_pending_event =
            *reinterpret_cast<XrEventDataInstanceLossPending*>(&event);
          m_exitRenderLoop = true;
          m_requestRestart = false;
          break;
        }
      }
      event = {XR_TYPE_EVENT_DATA_BUFFER, nullptr};
    }

    if (m_exitRenderLoop)
    {
      DeinitSession();
      DeinitSystem();
      if (m_requestRestart)
      {
        // Try to re-init session
        XrResult res = InitSystem();
        if (res != XR_SUCCESS)
        {
          return;
        }
        res = InitSession();
        if (res != XR_SUCCESS)
        {
          DeinitSystem();
          return;
        }
      }
    }
    //#TODO
  }
  else if (e.m_Type == ezGameApplicationExecutionEvent::Type::BeforePresent)
  {
    if (m_hView.IsInvalidated() || !m_sessionRunning)
      return;

#if EZ_ENABLED(EZ_PLATFORM_WINDOWS)
    ezGALDeviceDX11* pDevice = static_cast<ezGALDeviceDX11*>(ezGALDevice::GetDefaultDevice());
    ezGALContextDX11* pGALContext = static_cast<ezGALContextDX11*>(pDevice->GetPrimaryContext());
    if (m_eyeDesc.m_SampleCount == ezGALMSAASampleCount::None)
    {
      const ezGALTextureDX11* pSource = static_cast<const ezGALTextureDX11*>(pDevice->GetTexture(m_hColorRT));
      ID3D11Resource* dxSource = pSource->GetDXTexture();
      pGALContext->GetDXContext()->CopyResource(dxSource, m_colorSwapChainImagesD3D11[m_colorSwapchain.imageIndex].texture);
    }
    else
    {
      const ezGALTextureDX11* pSource = static_cast<const ezGALTextureDX11*>(pDevice->GetTexture(m_hColorRT));
      ID3D11Resource* dxSource = pSource->GetDXTexture();
      ezUInt32 srcSubResource = D3D11CalcSubresource(0, 0, 1);
      ezUInt32 dstSubResource = D3D11CalcSubresource(0, 0, 1);
      pGALContext->GetDXContext()->ResolveSubresource(m_colorSwapChainImagesD3D11[m_colorSwapchain.imageIndex].texture,
        dstSubResource, dxSource, srcSubResource, static_cast<DXGI_FORMAT>(m_colorSwapchain.format));
      srcSubResource = D3D11CalcSubresource(0, 1, 1);
      dstSubResource = D3D11CalcSubresource(0, 1, 1);
      pGALContext->GetDXContext()->ResolveSubresource(m_colorSwapChainImagesD3D11[m_colorSwapchain.imageIndex].texture,
        dstSubResource, dxSource, srcSubResource, static_cast<DXGI_FORMAT>(m_colorSwapchain.format));
    }
#else
    EZ_ASSERT_NOT_IMPLEMENTED;
#endif

    for (uint32_t i = 0; i < 2; i++)
    {
      m_projectionLayerViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
      m_projectionLayerViews[i].pose = m_views[i].pose;
      m_projectionLayerViews[i].fov = m_views[i].fov;
      m_projectionLayerViews[i].subImage.swapchain = m_colorSwapchain.handle;
      m_projectionLayerViews[i].subImage.imageRect.offset = {0, 0};
      m_projectionLayerViews[i].subImage.imageRect.extent = {(ezInt32)m_Info.m_vEyeRenderTargetSize.x, (ezInt32)m_Info.m_vEyeRenderTargetSize.y};
      m_projectionLayerViews[i].subImage.imageArrayIndex = i;

      if (m_supportsDepth)
      {
        m_depthLayerViews[i] = {XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR};
        m_depthLayerViews[i].minDepth = 0;
        m_depthLayerViews[i].maxDepth = 1;
        m_depthLayerViews[i].nearZ = 0; //TODO
        m_depthLayerViews[i].farZ = 1;  //TODO
        m_depthLayerViews[i].subImage.swapchain = m_depthSwapchain.handle;
        m_depthLayerViews[i].subImage.imageRect.offset = {0, 0};
        m_depthLayerViews[i].subImage.imageRect.extent = {(ezInt32)m_Info.m_vEyeRenderTargetSize.x, (ezInt32)m_Info.m_vEyeRenderTargetSize.y};
        m_depthLayerViews[i].subImage.imageArrayIndex = i;

        m_projectionLayerViews[i].next = &m_depthLayerViews[i];
      }
    }
    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    XR_CHECK_LOG(xrReleaseSwapchainImage(m_colorSwapchain.handle, &releaseInfo));
    if (m_supportsDepth)
    {
      XR_CHECK_LOG(xrReleaseSwapchainImage(m_depthSwapchain.handle, &releaseInfo));
    }

    //XrSwapchainImageBaseHeader* swapchainImage = reinterpret_cast<XrSwapchainImageBaseHeader*>(&m_colorSwapChainImagesD3D11[m_swapchainImageIndex]);
    m_layer.space = m_sceneSpace;
    m_layer.viewCount = 2;
    m_layer.views = m_projectionLayerViews;

    ezHybridArray<XrCompositionLayerBaseHeader*, 1> layers;
    layers.PushBack(reinterpret_cast<XrCompositionLayerBaseHeader*>(&m_layer));

    // Submit the composition layers for the predicted display time.
    XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
    frameEndInfo.displayTime = m_frameState.predictedDisplayTime;
    frameEndInfo.environmentBlendMode = m_blendMode;
    frameEndInfo.layerCount = layers.GetCapacity();
    frameEndInfo.layers = layers.GetData();

    XrResult result = xrEndFrame(m_session, &frameEndInfo);
    if (result != XR_SUCCESS)
    {
      //TODO?
    }
  }
  else if (e.m_Type == ezGameApplicationExecutionEvent::Type::BeginAppTick)
  {
  }
  else if (e.m_Type == ezGameApplicationExecutionEvent::Type::AfterPresent)
  {
    // This tells the compositor we submitted the frames are done rendering to them this frame.
    //#TODO vr::VRCompositor()->PostPresentHandoff();

    ezGALDevice* pDevice = ezGALDevice::GetDefaultDevice();
    ezGALContext* pGALContext = pDevice->GetPrimaryContext();
    ezRenderContext* m_pRenderContext = ezRenderContext::GetDefaultInstance();

    if (const ezGALTexture* tex = pDevice->GetTexture(m_hCompanionRenderTarget))
    {
      // We are rendering the companion window at the very start of the frame, using the content
      // of the last frame. That way we do not add additional delay before submitting the frames.
      EZ_PROFILE_AND_MARKER(pGALContext, "VR CompanionView");

      m_pRenderContext->BindMeshBuffer(ezGALBufferHandle(), ezGALBufferHandle(), nullptr, ezGALPrimitiveTopology::Triangles, 1);
      m_pRenderContext->BindConstantBuffer("ezVRCompanionViewConstants", m_hCompanionConstantBuffer);
      m_pRenderContext->BindShader(m_hCompanionShader);

      auto hRenderTargetView = ezGALDevice::GetDefaultDevice()->GetDefaultRenderTargetView(m_hCompanionRenderTarget);
      ezVec2 targetSize = ezVec2((float)tex->GetDescription().m_uiWidth, (float)tex->GetDescription().m_uiHeight);

      ezGALRenderTargetSetup renderTargetSetup;
      renderTargetSetup.SetRenderTarget(0, hRenderTargetView);
      pGALContext->SetRenderTargetSetup(renderTargetSetup);
      pGALContext->SetViewport(ezRectFloat(targetSize.x, targetSize.y));

      auto* constants = ezRenderContext::GetConstantBufferData<ezVRCompanionViewConstants>(m_hCompanionConstantBuffer);
      constants->TargetSize = targetSize;

      ezGALResourceViewHandle hInputView = pDevice->GetDefaultResourceView(m_hColorRT);
      m_pRenderContext->BindTexture2D("VRTexture", hInputView);
      m_pRenderContext->DrawMeshBuffer();
    }
  }
}

void ezOpenXR::OnBeginRender(ezUInt64)
{
  // TODO: Ideally we would like to call UpdatePoses() here and block and in BeforeBeginFrame
  // we would predict the pose in two frames.
}

void ezOpenXR::ReadHMDInfo()
{
  EZ_ASSERT_DEV(IsInitialized(), "Need to call 'Initialize' first.");

  //m_Info.m_sDeviceName = GetTrackedDeviceString(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
  //m_Info.m_sDeviceDriver = GetTrackedDeviceString(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);
  //m_pHMD->GetRecommendedRenderTargetSize(&m_Info.m_vEyeRenderTargetSize.x, &m_Info.m_vEyeRenderTargetSize.y);
  //m_Info.m_mat4eyePosLeft = GetHMDEyePose(vr::Eye_Left);
  //m_Info.m_mat4eyePosRight = GetHMDEyePose(vr::Eye_Right);
}


void ezOpenXR::OnDeviceActivated(ezVRDeviceID uiDeviceID)
{
  /*m_DeviceState[uiDeviceID].m_bDeviceIsConnected = true;
  switch (m_pHMD->GetTrackedDeviceClass(uiDeviceID))
  {
    case vr::TrackedDeviceClass_HMD:
      m_DeviceState[uiDeviceID].m_Type = ezVRDeviceState::Type::HMD;
      break;
    case vr::TrackedDeviceClass_Controller:
      m_DeviceState[uiDeviceID].m_Type = ezVRDeviceState::Type::Controller;

      break;
    case vr::TrackedDeviceClass_GenericTracker:
      m_DeviceState[uiDeviceID].m_Type = ezVRDeviceState::Type::Tracker;
      break;
    case vr::TrackedDeviceClass_TrackingReference:
      m_DeviceState[uiDeviceID].m_Type = ezVRDeviceState::Type::Reference;
      break;
    default:
      m_DeviceState[uiDeviceID].m_Type = ezVRDeviceState::Type::Unknown;
      break;
  }*/

  UpdateHands();

  {
    ezVRDeviceEvent e;
    e.m_Type = ezVRDeviceEvent::Type::DeviceAdded;
    e.uiDeviceID = uiDeviceID;
    m_DeviceEvents.Broadcast(e);
  }
}


void ezOpenXR::OnDeviceDeactivated(ezVRDeviceID uiDeviceID)
{
  /*m_DeviceState[uiDeviceID].m_bDeviceIsConnected = false;
  m_DeviceState[uiDeviceID].m_bPoseIsValid = false;
  UpdateHands();
  {
    ezVRDeviceEvent e;
    e.m_Type = ezVRDeviceEvent::Type::DeviceRemoved;
    e.uiDeviceID = uiDeviceID;
    m_DeviceEvents.Broadcast(e);
  }*/
}

void ezOpenXR::UpdatePoses()
{
  EZ_ASSERT_DEV(IsInitialized(), "Need to call 'Initialize' first.");

  ezStopwatch sw;

  UpdateHands();

  XrViewState viewState{XR_TYPE_VIEW_STATE};
  ezUInt32 viewCapacityInput = 2;
  ezUInt32 viewCountOutput;

  XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
  viewLocateInfo.displayTime = m_frameState.predictedDisplayTime;
  viewLocateInfo.space = m_sceneSpace;
  XrResult res = xrLocateViews(m_session, &viewLocateInfo, &viewState, viewCapacityInput, &viewCountOutput, m_views);
  if (res != XR_SUCCESS)
  {
    m_DeviceState[0].m_bPoseIsValid = false;
    return;
  }

  // Update camera projection
  {
    const float fAspectRatio = (float)m_Info.m_vEyeRenderTargetSize.x / (float)m_Info.m_vEyeRenderTargetSize.y;
    auto CreateProjection = [](const XrView& view, ezCamera* cam) {
      return ezGraphicsUtils::CreatePerspectiveProjectionMatrix(ezMath::Tan(ezAngle::Radian(view.fov.angleLeft)),
        ezMath::Tan(ezAngle::Radian(view.fov.angleRight)),
        ezMath::Tan(ezAngle::Radian(view.fov.angleDown)),
        ezMath::Tan(ezAngle::Radian(view.fov.angleUp)),
        cam->GetNearPlane(),
        cam->GetFarPlane());
    };

    // Update projection with newest near/ far values. If not sync camera is set, just use the last value from VR camera.
    const ezMat4 projLeft = CreateProjection(m_views[0], m_pCameraToSynchronize ? m_pCameraToSynchronize : &m_VRCamera);
    const ezMat4 projRight = CreateProjection(m_views[1], m_pCameraToSynchronize ? m_pCameraToSynchronize : &m_VRCamera);
    m_VRCamera.SetStereoProjection(projLeft, projRight, fAspectRatio);
  }

  // Update camera view
  ezMat4 cameraCenterView;
  {
    ezTransform add;
    add.SetIdentity();
    ezView* pView = nullptr;
    if (ezRenderWorld::TryGetView(m_hView, pView))
    {
      if (const ezWorld* pWorld = pView->GetWorld())
      {
        EZ_LOCK(pWorld->GetReadMarker());
        if (const ezStageSpaceComponentManager* pStageMan = pWorld->GetComponentManager<ezStageSpaceComponentManager>())
        {
          if (const ezStageSpaceComponent* pStage = pStageMan->GetSingletonComponent())
          {
            auto stageSpace = pStage->GetStageSpace();
            if (m_StageSpace != stageSpace)
              SetStageSpace(pStage->GetStageSpace());
            add = pStage->GetOwner()->GetGlobalTransform();
          }
        }
      }
    }

    // First compute XR space center view matrix (for device state pose)
    const ezMat4 viewLeft = ConvertPoseToMatrix(m_views[0].pose);
    const ezMat4 viewRight = ConvertPoseToMatrix(m_views[1].pose);
    m_VRCamera.SetViewMatrix(viewLeft, ezCameraEye::Left);
    m_VRCamera.SetViewMatrix(viewRight, ezCameraEye::Right);
    cameraCenterView = ezGraphicsUtils::CreateViewMatrix(m_VRCamera.GetCenterPosition(),
      m_VRCamera.GetCenterDirForwards(), m_VRCamera.GetCenterDirRight(), m_VRCamera.GetCenterDirUp());

    // Then set shifted view matrix
    const ezMat4 mAdd = add.GetAsMat4();
    ezMat4 mShiftedPos = /*m_DeviceState[0].m_mPose * */ mAdd;
    mShiftedPos.Invert();

    const ezMat4 mViewTransformLeft = viewLeft * mShiftedPos;
    const ezMat4 mViewTransformRight = viewRight * mShiftedPos;
    m_VRCamera.SetViewMatrix(mViewTransformLeft, ezCameraEye::Left);
    m_VRCamera.SetViewMatrix(mViewTransformRight, ezCameraEye::Right);
  }

  // Update state
  m_DeviceState[0].m_mPose = cameraCenterView;
  m_DeviceState[0].m_vPosition = m_DeviceState[0].m_mPose.GetTranslationVector();
  m_DeviceState[0].m_qRotation.SetFromMat3(m_DeviceState[0].m_mPose.GetRotationalPart());
  m_DeviceState[0].m_vVelocity.SetZero();
  m_DeviceState[0].m_vAngularVelocity.SetZero();
  //m_hmdState.m_Type = ezVRDeviceState::Type::HMD;
  m_DeviceState[0].m_bPoseIsValid = true;
  m_DeviceState[0].m_bDeviceIsConnected = true;

  if (m_pCameraToSynchronize)
  {
    UpdateCamera();

    // put the camera orientation into the sound listener and enable the listener override mode
    if (ezSoundInterface* pSoundInterface = ezSingletonRegistry::GetSingletonInstance<ezSoundInterface>())
    {
      pSoundInterface->SetListener(
        -1, m_VRCamera.GetCenterPosition(), m_VRCamera.GetCenterDirForwards(), m_VRCamera.GetCenterDirUp(), ezVec3::ZeroVector());
    }
  }
}

void ezOpenXR::UpdateHands()
{


  //m_iLeftControllerDeviceID = m_pHMD->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
  //m_iRightControllerDeviceID = m_pHMD->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);
}

void ezOpenXR::SetStageSpace(ezVRStageSpace::Enum space)
{
  m_StageSpace = space;
  //switch (space)
  //{
  //  case ezVRStageSpace::Seated:
  //    vr::VRCompositor()->SetTrackingSpace(vr::TrackingUniverseOrigin::TrackingUniverseSeated);
  //    break;
  //  case ezVRStageSpace::Standing:
  //    vr::VRCompositor()->SetTrackingSpace(vr::TrackingUniverseOrigin::TrackingUniverseStanding);
  //    break;
  //}
}

void ezOpenXR::SetHMDCamera(ezCamera* pCamera)
{
  EZ_ASSERT_DEV(IsInitialized(), "Need to call 'Initialize' first.");

  if (m_pCameraToSynchronize == pCamera)
    return;

  m_pCameraToSynchronize = pCamera;
  if (m_pCameraToSynchronize)
  {
    m_uiSettingsModificationCounter = m_pCameraToSynchronize->GetSettingsModificationCounter() + 1;
    m_VRCamera = *m_pCameraToSynchronize;
    m_VRCamera.SetCameraMode(ezCameraMode::Stereo, 90.0f, m_pCameraToSynchronize->GetNearPlane(), m_pCameraToSynchronize->GetFarPlane());
    UpdateCamera();
  }

  if (ezSoundInterface* pSoundInterface = ezSingletonRegistry::GetSingletonInstance<ezSoundInterface>())
  {
    pSoundInterface->SetListenerOverrideMode(m_pCameraToSynchronize != nullptr);
  }
}


void ezOpenXR::UpdateCamera()
{
  if (m_uiSettingsModificationCounter != m_pCameraToSynchronize->GetSettingsModificationCounter())
  {
    //const float fAspectRatio = (float)m_Info.m_vEyeRenderTargetSize.x / (float)m_Info.m_vEyeRenderTargetSize.y;
    /* ezMat4 projLeft =
        GetHMDProjectionEye(vr::Hmd_Eye::Eye_Left, m_pCameraToSynchronize->GetNearPlane(), m_pCameraToSynchronize->GetFarPlane());
    ezMat4 projRight =
        GetHMDProjectionEye(vr::Hmd_Eye::Eye_Right, m_pCameraToSynchronize->GetNearPlane(), m_pCameraToSynchronize->GetFarPlane());
    m_VRCamera.SetStereoProjection(projLeft, projRight, fAspectRatio);*/
  }
}

XrPosef ezOpenXR::ConvertTransform(const ezTransform& tr)
{
  XrPosef pose;
  pose.orientation = ConvertOrientation(tr.m_qRotation);
  pose.position = ConvertPosition(tr.m_vPosition);
  return pose;
}

XrQuaternionf ezOpenXR::ConvertOrientation(const ezQuat& q)
{
  return {q.v.x, q.v.z, q.v.y, -q.w};
}

XrVector3f ezOpenXR::ConvertPosition(const ezVec3& pos)
{
  return {pos.x, pos.z, pos.y};
}

ezQuat ezOpenXR::ConvertOrientation(const XrQuaternionf& q)
{
  return {q.x, q.z, q.y, -q.w};
}

ezVec3 ezOpenXR::ConvertPosition(const XrVector3f& pos)
{
  return {pos.x, pos.z, pos.y};
}

ezMat4 ezOpenXR::ConvertPoseToMatrix(const XrPosef& pose)
{
  ezMat4 m;
  ezMat3 rot = ConvertOrientation(pose.orientation).GetAsMat3();
  ezVec3 pos = ConvertPosition(pose.position);
  m.SetTransformationMatrix(rot, pos);
  return m;
}

//
//ezMat4 ezOpenXR::GetHMDProjectionEye(vr::Hmd_Eye nEye, float fNear, float fFar) const
//{
//  EZ_ASSERT_DEV(m_pHMD, "Need to call 'Initialize' first.");
//
//  float Left, Right, Top, Bottom;
//  m_pHMD->GetProjectionRaw(nEye, &Left, &Right, &Top, &Bottom);
//  ezMat4 proj;
//  proj.SetPerspectiveProjectionMatrix(Left * fNear, Right * fNear, Top * fNear, Bottom * fNear, fNear, fFar);
//  return proj;
//}
//
//ezMat4 ezOpenXR::GetHMDEyePose(vr::Hmd_Eye nEye) const
//{
//  EZ_ASSERT_DEV(m_pHMD, "Need to call 'Initialize' first.");
//
//  vr::HmdMatrix34_t matEyeRight = m_pHMD->GetEyeToHeadTransform(nEye);
//  ezMat4 matrixObj = ConvertSteamVRMatrix(matEyeRight);
//  matrixObj.Invert();
//  return matrixObj;
//}
//
//ezString ezOpenXR::GetTrackedDeviceString(
//    vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError* peError) const
//{
//  EZ_ASSERT_DEV(m_pHMD, "Need to call 'Initialize' first.");
//
//  const ezUInt32 uiCharCount = m_pHMD->GetStringTrackedDeviceProperty(unDevice, prop, nullptr, 0, peError);
//  ezHybridArray<char, 128> temp;
//  temp.SetCountUninitialized(uiCharCount);
//  if (uiCharCount > 0)
//  {
//    m_pHMD->GetStringTrackedDeviceProperty(unDevice, prop, temp.GetData(), uiCharCount, peError);
//  }
//  else
//  {
//    temp.SetCount(1);
//    temp[0] = 0;
//  }
//  return ezString(temp.GetData());
//}
//
//ezMat4 ezOpenXR::ConvertSteamVRMatrix(const vr::HmdMatrix34_t& matPose)
//{
//  // clang-format off
//  // Convert right handed to left handed with Y and Z swapped.
//  // Same as A^t * matPose * A with A being identity with y and z swapped.
//  ezMat4 mMat(
//    matPose.m[0][0], matPose.m[0][2], matPose.m[0][1], matPose.m[0][3],
//    matPose.m[2][0], matPose.m[2][2], matPose.m[2][1], matPose.m[2][3],
//    matPose.m[1][0], matPose.m[1][2], matPose.m[1][1], matPose.m[1][3],
//    0, 0, 0, 1.0f);
//
//  return mMat;
//  // clang-format on
//}
//
//ezVec3 ezOpenXR::ConvertSteamVRVector(const vr::HmdVector3_t& vector)
//{
//  return ezVec3(vector.v[0], vector.v[2], vector.v[1]);
//}

EZ_STATICLINK_FILE(OpenXRPlugin, OpenXRPlugin_OpenXRSingleton);
