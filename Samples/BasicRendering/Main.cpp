#include <Foundation/Configuration/Startup.h>
#include <Foundation/IO/FileSystem/FileSystem.h>
#include <Foundation/IO/FileSystem/DataDirTypeFolder.h>
#include <Foundation/IO/FileSystem/FileReader.h>
#include <Foundation/Strings/PathUtils.h>
#include <Foundation/Strings/StringBuilder.h>
#include <Foundation/Containers/Map.h>
#include <Foundation/Strings/String.h>
#include <Foundation/Logging/Log.h>
#include <Foundation/Logging/ConsoleWriter.h>
#include <Foundation/Logging/VisualStudioWriter.h>
#include <Foundation/Logging/HTMLWriter.h>
#include <Foundation/Time/Clock.h>
#include <Foundation/Communication/Telemetry.h>
#include <Foundation/Threading/TaskSystem.h>

#include <Core/Basics.h>
#include <Core/Application/Application.h>
#include <Core/Input/InputManager.h>
#include <Core/ResourceManager/ResourceManager.h>

#include <System/Window/Window.h>

#define DEMO_GL EZ_OFF
#define DEMO_DX11 EZ_ON

#if EZ_ENABLED(DEMO_GL)
#include <RendererGL/Device/DeviceGL.h>
#else
#include <RendererDX11/Device/DeviceDX11.h>
#endif

#include <RendererFoundation/Device/SwapChain.h>
#include <RendererFoundation/Context/Context.h>

#include <Helper/MayaObj.h>

#include <CoreUtils/Debugging/DataTransfer.h>

#include <RendererCore/RendererCore.h>
#include <RendererCore/ShaderCompiler/ShaderCompiler.h>

// This sample is a really simple low-level rendering demo showing how the high level renderer will interact with the GPU abstraction layer

class TestWindow : public ezWindow
{
public:

  TestWindow()
    : ezWindow()
  {
    m_bCloseRequested = false;
  }

  virtual void OnClickCloseMessage() override
  {
    m_bCloseRequested = true;
  }

  bool m_bCloseRequested;
};

struct TestCB
{
  ezMat4 mvp;
};

static bool g_bMSAA = false;
static ezUInt32 g_uiWindowWidth = 1280;
static ezUInt32 g_uiWindowHeight = 720;

class BasicRendering : public ezApplication
{
public:

  void AfterEngineInit() override
  {
    m_iCurObject = 0;

    ezFileSystem::RegisterDataDirectoryFactory(ezDataDirectory::FolderType::Factory);
    ezFileSystem::AddDataDirectory("");

    ezStringBuilder sReadDir = BUILDSYSTEM_OUTPUT_FOLDER;
    sReadDir.AppendPath("../../Shared/FreeContent/Basic Rendering/");

    ezFileSystem::RegisterDataDirectoryFactory(ezDataDirectory::FolderType::Factory);
    ezFileSystem::AddDataDirectory(sReadDir.GetData(), ezFileSystem::AllowWrites, "Basic Rendering Content");

    ezGlobalLog::AddLogWriter(ezLogWriter::Console::LogMessageHandler);

    ezTelemetry::CreateServer();
    ezPlugin::LoadPlugin("ezInspectorPlugin");

#if EZ_ENABLED(DEMO_GL)
    EZ_VERIFY(ezPlugin::LoadPlugin("ezShaderCompilerGLSL").Succeeded(), "Compiler Plugin not found");
#else
    EZ_VERIFY(ezPlugin::LoadPlugin("ezShaderCompilerHLSL").Succeeded(), "Compiler Plugin not found");
#endif

    ezClock::SetNumGlobalClocks();

    // Register escape key
    ezInputActionConfig cfg;

    cfg = ezInputManager::GetInputActionConfig("Main", "CloseApp");
    cfg.m_sInputSlotTrigger[0] = ezInputSlot_KeyEscape;
    ezInputManager::SetInputActionConfig("Main", "CloseApp", cfg, true);

    cfg = ezInputManager::GetInputActionConfig("Main", "ToggleShader");
    cfg.m_sInputSlotTrigger[0] = ezInputSlot_KeySpace;
    ezInputManager::SetInputActionConfig("Main", "ToggleShader", cfg, true);

    cfg = ezInputManager::GetInputActionConfig("Main", "PreloadShader");
    cfg.m_sInputSlotTrigger[0] = ezInputSlot_KeyP;
    ezInputManager::SetInputActionConfig("Main", "PreloadShader", cfg, true);

    cfg = ezInputManager::GetInputActionConfig("Main", "NextObj");
    cfg.m_sInputSlotTrigger[0] = ezInputSlot_KeyP;
    ezInputManager::SetInputActionConfig("Main", "NextObj", cfg, true);

    cfg = ezInputManager::GetInputActionConfig("Main", "PrevObj");
    cfg.m_sInputSlotTrigger[0] = ezInputSlot_KeyO;
    ezInputManager::SetInputActionConfig("Main", "PrevObj", cfg, true);

    // Create a window for rendering
    ezWindowCreationDesc WindowCreationDesc;
    WindowCreationDesc.m_ClientAreaSize.width = g_uiWindowWidth;
    WindowCreationDesc.m_ClientAreaSize.height = g_uiWindowHeight;
    m_pWindow = EZ_DEFAULT_NEW(TestWindow)();
    m_pWindow->Initialize(WindowCreationDesc);


    // Create a device
    ezGALDeviceCreationDescription DeviceInit;
    DeviceInit.m_bCreatePrimarySwapChain = true;
    DeviceInit.m_bDebugDevice = true;
    DeviceInit.m_PrimarySwapChainDescription.m_pWindow = m_pWindow;
    DeviceInit.m_PrimarySwapChainDescription.m_SampleCount = g_bMSAA ? ezGALMSAASampleCount::FourSamples : ezGALMSAASampleCount::None;
    DeviceInit.m_PrimarySwapChainDescription.m_bCreateDepthStencilBuffer = true;
    DeviceInit.m_PrimarySwapChainDescription.m_DepthStencilBufferFormat = ezGALResourceFormat::D24S8;
    DeviceInit.m_PrimarySwapChainDescription.m_bAllowScreenshots = true;
    DeviceInit.m_PrimarySwapChainDescription.m_bVerticalSynchronization = true;

#if EZ_ENABLED(DEMO_GL)
    m_pDevice = EZ_DEFAULT_NEW(ezGALDeviceGL)(DeviceInit);
#else
    m_pDevice = EZ_DEFAULT_NEW(ezGALDeviceDX11)(DeviceInit);
#endif
    EZ_VERIFY(m_pDevice->Init() == EZ_SUCCESS, "Device init failed!");

    ezGALDevice::SetDefaultDevice(m_pDevice);

    // Get the primary swapchain (this one will always be created by device init except if the user instructs no swap chain creation explicitly)
    ezGALSwapChainHandle hPrimarySwapChain = m_pDevice->GetPrimarySwapChain();
    const ezGALSwapChain* pPrimarySwapChain = m_pDevice->GetSwapChain(hPrimarySwapChain);

    m_hBBRT = pPrimarySwapChain->GetRenderTargetViewConfig();


    // Create a constant buffer for matrix upload
    m_hCB = m_pDevice->CreateConstantBuffer(sizeof(TestCB));

    for (int i = 0; i < MaxObjs; ++i)
      m_pObj[i] = DontUse::MayaObj::LoadFromFile("ez.obj", m_pDevice, i);

#if EZ_ENABLED(DEMO_GL)
    ezRendererCore::SetShaderPlatform("GL3", true);
#else
    ezRendererCore::SetShaderPlatform("DX11_SM40", true);
#endif

    m_hShader = ezResourceManager::LoadResource<ezShaderResource>("Shaders/ez2.shader");

    ezRendererCore::SetActiveShader(m_hShader);
    ezRendererCore::SetShaderPermutationVariable("COLORED", "1");

    ezGALRasterizerStateCreationDescription RasterStateDesc;
    RasterStateDesc.m_bWireFrame = true;
    RasterStateDesc.m_CullMode = ezGALCullMode::Back;
    RasterStateDesc.m_bFrontCounterClockwise = true;
    m_hRasterizerState = m_pDevice->CreateRasterizerState(RasterStateDesc);
    EZ_ASSERT(!m_hRasterizerState.IsInvalidated(), "Couldn't create rasterizer state!");


    ezGALDepthStencilStateCreationDescription DepthStencilStateDesc;
    DepthStencilStateDesc.m_bDepthTest = true;
    DepthStencilStateDesc.m_bDepthWrite = true;
    m_hDepthStencilState = m_pDevice->CreateDepthStencilState(DepthStencilStateDesc);
    EZ_ASSERT(!m_hDepthStencilState.IsInvalidated(), "Couldn't create depth-stencil state!");

    m_DebugBackBufferDT.EnableDataTransfer("Back Buffer");

    ezStartup::StartupEngine();
  }


  ApplicationExecution Run() override
  {
    m_pWindow->ProcessWindowMessages();

    if (m_pWindow->m_bCloseRequested)
      return ApplicationExecution::Quit;

    if (ezInputManager::GetInputActionState("Main", "CloseApp") == ezKeyState::Pressed)
      return ApplicationExecution::Quit;

    if (ezInputManager::GetInputActionState("Main", "ToggleShader") == ezKeyState::Pressed)
    {
      static int iPerm = 0;
      static int iColorValue = 10;

      switch (iPerm)
      {
      case 0:
        ezRendererCore::SetShaderPermutationVariable("COLORED", "0");
        break;
      case 1:
        ezRendererCore::SetShaderPermutationVariable("COLORED", "1");

        if (iColorValue > 250)
          iColorValue = 10;

        ezRendererCore::SetShaderPermutationVariable("COLORVALUE", ezConversionUtils::ToString(iColorValue).GetData());

        iColorValue += 10;

        break;
      default:
        iPerm = 0;
        break;
      }

      ++iPerm;
      iPerm %= 2;
    }

    if (ezInputManager::GetInputActionState("Main", "PreloadShader") == ezKeyState::Pressed)
    {
      ezPermutationGenerator All;
      All.ReadFromFile("ShaderPermutations.txt", ezRendererCore::GetShaderPlatform().GetData());

      ezRendererCore::PreloadShaderPermutations(m_hShader, All, ezTime::Milliseconds(10000.0));
    }

    if (ezInputManager::GetInputActionState("Main", "NextObj") == ezKeyState::Pressed)
    {
      m_iCurObject = (m_iCurObject + 1) % MaxObjs;
    }

    if (ezInputManager::GetInputActionState("Main", "PrevObj") == ezKeyState::Pressed)
    {
      m_iCurObject = m_iCurObject == 0 ? (MaxObjs - 1) : m_iCurObject - 1;
    }

    ezClock::UpdateAllGlobalClocks();

    ezInputManager::Update(ezClock::Get()->GetTimeDiff());

    ezTelemetry::PerFrameUpdate();

    ezTaskSystem::FinishFrameTasks();


    // Before starting to render in a frame call this function
    m_pDevice->BeginFrame();

    // The ezGALContext class is the main interaction point for draw / compute operations
    ezGALContext* pContext = m_pDevice->GetPrimaryContext();

    pContext->SetRenderTargetConfig(m_hBBRT);
    pContext->SetViewport(0.0f, 0.0f, (float) g_uiWindowWidth, (float) g_uiWindowHeight, 0.0f, 1.0f);
    pContext->Clear(ezColor::GetBlack());

    pContext->SetRasterizerState(m_hRasterizerState);
    pContext->SetDepthStencilState(m_hDepthStencilState);

    static float fRotY = 0.0f;
    fRotY -= 30.0f * ezClock::Get()->GetTimeDiff().AsFloat();

    ezMat4 ModelRot;
    ModelRot.SetRotationMatrixY(ezAngle::Degree(fRotY));

    ezMat4 Model;
    Model.SetIdentity();
    Model.SetScalingFactors(ezVec3(0.75f));


    ezMat4 View;
    View.SetIdentity();
    View.SetLookAtMatrix(ezVec3(0.5f, 1.5f, 2.0f), ezVec3(0.0f, 0.5f, 0.0f), ezVec3(0.0f, 1.0f, 0.0f));

    ezMat4 Proj;
    Proj.SetIdentity();
    Proj.SetPerspectiveProjectionMatrixFromFovY(ezAngle::Degree(80.0f), (float) g_uiWindowWidth / (float) g_uiWindowHeight, 0.1f, 1000.0f,
#if EZ_ENABLED(DEMO_GL)
      ezProjectionDepthRange::MinusOneToOne
#else
      ezProjectionDepthRange::ZeroToOne
#endif
      );

    TestCB ObjectData;

    ObjectData.mvp = Proj * View * Model * ModelRot;

    pContext->UpdateBuffer(m_hCB, 0, &ObjectData, sizeof(TestCB));

    pContext->SetConstantBuffer(1, m_hCB);

    ezRendererCore::DrawMeshBuffer(pContext, m_pObj[m_iCurObject]->m_hMeshBuffer);


    // Readback: Currently not supported for MSAA since Resolve() is not implemented
    if (m_DebugBackBufferDT.IsTransferRequested() && !g_bMSAA)
    {
      ezGALTextureHandle hBBTexture = m_pDevice->GetSwapChain(m_pDevice->GetPrimarySwapChain())->GetBackBufferTexture();
      pContext->ReadbackTexture(hBBTexture);

      ezArrayPtr<ezUInt8> pImageContent = EZ_DEFAULT_NEW_ARRAY(ezUInt8, 4 * g_uiWindowWidth * g_uiWindowHeight);

      ezGALSystemMemoryDescription MemDesc;
      MemDesc.m_pData = pImageContent.GetPtr();
      MemDesc.m_uiRowPitch = 4 * g_uiWindowWidth;
      MemDesc.m_uiSlicePitch = 4 * g_uiWindowWidth * g_uiWindowHeight;

      ezArrayPtr<ezGALSystemMemoryDescription> SysMemDescs(&MemDesc, 1);

      pContext->CopyTextureReadbackResult(hBBTexture, &SysMemDescs);

      ezDataTransferObject DataObject(m_DebugBackBufferDT, "Back Buffer", "image/rgba8", "rgba");
      DataObject.GetWriter() << g_uiWindowWidth;
      DataObject.GetWriter() << g_uiWindowHeight;
      DataObject.GetWriter().WriteBytes(pImageContent.GetPtr(), 4 * g_uiWindowWidth * g_uiWindowHeight);
      DataObject.Transmit();

      EZ_DEFAULT_DELETE_ARRAY(pImageContent);
    }


    m_pDevice->Present(m_pDevice->GetPrimarySwapChain());

    m_pDevice->EndFrame();

    return ezApplication::Continue;
  }

  void BeforeEngineShutdown() override
  {
    for (int i = 0; i < MaxObjs; ++i)
      EZ_DEFAULT_DELETE(m_pObj[i]);

    m_hShader.Invalidate();

    ezStartup::ShutdownEngine();

    m_pDevice->Shutdown();

    // the device requires some data for shutdown that is referenced below
    // so we must not clean this stuff up before 'device shutdown'
    m_pDevice->DestroyBuffer(m_hCB);
    m_pDevice->DestroyRasterizerState(m_hRasterizerState);
    m_pDevice->DestroyDepthStencilState(m_hDepthStencilState);

    EZ_DEFAULT_DELETE(m_pDevice);

    m_pWindow->Destroy();
    EZ_DEFAULT_DELETE(m_pWindow);
  }

private:

  TestWindow* m_pWindow;

  ezGALDevice* m_pDevice;

  ezGALRenderTargetConfigHandle m_hBBRT;

  ezGALBufferHandle m_hCB;

  ezGALRasterizerStateHandle m_hRasterizerState;

  ezGALDepthStencilStateHandle m_hDepthStencilState;

  ezShaderResourceHandle m_hShader;

  static const int MaxObjs = 7;

  ezInt32 m_iCurObject;
  DontUse::MayaObj* m_pObj[MaxObjs];

  ezDataTransfer m_DebugBackBufferDT;
};

EZ_CONSOLEAPP_ENTRY_POINT(BasicRendering);
