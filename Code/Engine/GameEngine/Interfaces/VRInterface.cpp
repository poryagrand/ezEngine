#include <GameEnginePCH.h>

#include <GameEngine/Interfaces/VRInterface.h>
#include <Foundation/Reflection/Reflection.h>

// clang-format off
EZ_BEGIN_STATIC_REFLECTED_ENUM(ezVRDeviceType, 1)
EZ_BITFLAGS_CONSTANTS(ezVRDeviceType::HMD, ezVRDeviceType::LeftController, ezVRDeviceType::RightController)
EZ_BITFLAGS_CONSTANTS(ezVRDeviceType::DeviceID0, ezVRDeviceType::DeviceID1, ezVRDeviceType::DeviceID2, ezVRDeviceType::DeviceID3)
EZ_BITFLAGS_CONSTANTS(ezVRDeviceType::DeviceID4, ezVRDeviceType::DeviceID5, ezVRDeviceType::DeviceID6, ezVRDeviceType::DeviceID7)
EZ_BITFLAGS_CONSTANTS(ezVRDeviceType::DeviceID8, ezVRDeviceType::DeviceID9, ezVRDeviceType::DeviceID10, ezVRDeviceType::DeviceID11)
EZ_BITFLAGS_CONSTANTS(ezVRDeviceType::DeviceID12, ezVRDeviceType::DeviceID13, ezVRDeviceType::DeviceID14, ezVRDeviceType::DeviceID15)
EZ_END_STATIC_REFLECTED_ENUM;

EZ_BEGIN_STATIC_REFLECTED_ENUM(ezVRStageSpace, 1)
EZ_BITFLAGS_CONSTANTS(ezVRStageSpace::Seated, ezVRStageSpace::Standing)
EZ_END_STATIC_REFLECTED_ENUM;
// clang-format on

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezActorPluginWindowVR, 1, ezRTTINoAllocator);
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

EZ_STATICLINK_FILE(GameEngine, GameEngine_Interfaces_VRInterface);

//////////////////////////////////////////////////////////////////////////

ezWindowVR::ezWindowVR(ezVRInterface* pVrInterface, ezUniquePtr<ezWindowBase> pCompanionWindow)
  : m_pVrInterface(pVrInterface)
  , m_pCompanionWindow(std::move(pCompanionWindow))
{}

ezWindowVR::~ezWindowVR() {}

ezSizeU32 ezWindowVR::GetClientAreaSize() const
{
  return m_pVrInterface->GetHmdInfo().m_vEyeRenderTargetSize;
}

ezWindowHandle ezWindowVR::GetNativeWindowHandle() const
{
  //TODO
  return ezWindowHandle();
}

bool ezWindowVR::IsFullscreenWindow(bool bOnlyProperFullscreenMode) const
{
  return true;
}

void ezWindowVR::ProcessWindowMessages()
{
  //TODO
}

//////////////////////////////////////////////////////////////////////////

ezWindowOutputTargetVR::ezWindowOutputTargetVR(
  ezVRInterface* pVrInterface, ezUniquePtr<ezWindowOutputTargetBase> pCompanionWindowOutputTarget)
  : m_pVrInterface(pVrInterface)
  , m_pCompanionWindowOutputTarget(std::move(pCompanionWindowOutputTarget))
{
}

ezWindowOutputTargetVR::~ezWindowOutputTargetVR() {}

void ezWindowOutputTargetVR::Present(bool bEnableVSync)
{
  //TODO present VR

  if (m_pCompanionWindowOutputTarget)
  {
    m_pCompanionWindowOutputTarget->Present(false);
  }
}

ezResult ezWindowOutputTargetVR::CaptureImage(ezImage& out_Image)
{
  if (m_pCompanionWindowOutputTarget)
  {
    return m_pCompanionWindowOutputTarget->CaptureImage(out_Image);
  }
  return EZ_FAILURE;
}

//////////////////////////////////////////////////////////////////////////

ezActorPluginWindowVR::ezActorPluginWindowVR(ezVRInterface* pVrInterface, ezUniquePtr<ezWindowBase> companionWindow,
  ezUniquePtr<ezWindowOutputTargetBase> companionWindowOutput)
  : m_pVrInterface(pVrInterface)
{
  m_pWindow = EZ_DEFAULT_NEW(ezWindowVR, pVrInterface, std::move(companionWindow));
  m_pWindowOutputTarget = EZ_DEFAULT_NEW(ezWindowOutputTargetVR, pVrInterface, std::move(companionWindowOutput));
}

ezActorPluginWindowVR::~ezActorPluginWindowVR()
{
}

ezWindowBase* ezActorPluginWindowVR::GetWindow() const
{
  return m_pWindow.Borrow();
}

ezWindowOutputTargetBase* ezActorPluginWindowVR::GetOutputTarget() const
{
  return m_pWindowOutputTarget.Borrow();
}

void ezActorPluginWindowVR::Update()
{
  // TODO something
}
