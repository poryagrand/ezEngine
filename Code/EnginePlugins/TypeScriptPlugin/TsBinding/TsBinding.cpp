#include <TypeScriptPluginPCH.h>

#include <Core/ResourceManager/ResourceManager.h>
#include <Core/World/World.h>
#include <Duktape/duktape.h>
#include <Foundation/Profiling/Profiling.h>
#include <TypeScriptPlugin/TsBinding/TsBinding.h>

static ezHashTable<duk_context*, ezTypeScriptBinding*> s_DukToBinding;

ezSet<const ezRTTI*> ezTypeScriptBinding::s_RequiredEnums;
ezSet<const ezRTTI*> ezTypeScriptBinding::s_RequiredFlags;

static int __CPP_Time_Now(duk_context* pDuk)
{
  ezDuktapeFunction duk(pDuk);
  return duk.ReturnFloat(ezTime::Now().GetSeconds());
}

ezTypeScriptBinding::ezTypeScriptBinding()
  : m_Duk("Typescript Binding")
{
  s_DukToBinding[m_Duk] = this;
}

ezTypeScriptBinding::~ezTypeScriptBinding()
{
  s_DukToBinding.Remove(m_Duk);
}

ezResult ezTypeScriptBinding::Init_Time()
{
  m_Duk.RegisterGlobalFunction("__CPP_Time_Now", __CPP_Time_Now, 0);

  return EZ_SUCCESS;
}

ezTypeScriptBinding* ezTypeScriptBinding::RetrieveBinding(duk_context* pDuk)
{
  ezTypeScriptBinding* pBinding = nullptr;
  s_DukToBinding.TryGetValue(pDuk, pBinding);
  return pBinding;
}

ezResult ezTypeScriptBinding::Initialize(ezWorld& world)
{
  EZ_LOG_BLOCK("Initialize TypeScript Binding");
  EZ_PROFILE_SCOPE("Initialize TypeScript Binding");

  m_hScriptCompendium = ezResourceManager::LoadResource<ezScriptCompendiumResource>(":project/AssetCache/Common/Scripts.ezScriptCompendium");

  m_Duk.EnableModuleSupport(&ezTypeScriptBinding::DukSearchModule);
  m_Duk.StorePointerInStash("ezTypeScriptBinding", this);

  m_Duk.RegisterGlobalFunction("__CPP_Binding_RegisterMessageHandler", &ezTypeScriptBinding::__CPP_Binding_RegisterMessageHandler, 2);

  StoreWorld(&world);

  SetupRttiFunctionBindings();
  SetupRttiPropertyBindings();

  EZ_SUCCEED_OR_RETURN(Init_RequireModules());
  EZ_SUCCEED_OR_RETURN(Init_Log());
  EZ_SUCCEED_OR_RETURN(Init_Utils());
  EZ_SUCCEED_OR_RETURN(Init_Time());
  EZ_SUCCEED_OR_RETURN(Init_GameObject());
  EZ_SUCCEED_OR_RETURN(Init_FunctionBinding());
  EZ_SUCCEED_OR_RETURN(Init_PropertyBinding());
  EZ_SUCCEED_OR_RETURN(Init_Component());
  EZ_SUCCEED_OR_RETURN(Init_World());
  EZ_SUCCEED_OR_RETURN(Init_Clock());
  EZ_SUCCEED_OR_RETURN(Init_Debug());
  EZ_SUCCEED_OR_RETURN(Init_Random());
  EZ_SUCCEED_OR_RETURN(Init_Physics());

  m_bInitialized = true;
  return EZ_SUCCESS;
}

ezResult ezTypeScriptBinding::LoadComponent(const ezUuid& typeGuid, TsComponentTypeInfo& out_TypeInfo)
{
  if (!m_bInitialized || !typeGuid.IsValid())
  {
    return EZ_FAILURE;
  }

  // check if this component type has been loaded before
  {
    auto itLoaded = m_LoadedComponents.Find(typeGuid);

    if (itLoaded.IsValid())
    {
      out_TypeInfo = m_TsComponentTypes.Find(typeGuid);
      return itLoaded.Value() ? EZ_SUCCESS : EZ_FAILURE;
    }
  }

  EZ_PROFILE_SCOPE("Load Script Component");

  bool& bLoaded = m_LoadedComponents[typeGuid];
  bLoaded = false;

  ezResourceLock<ezScriptCompendiumResource> pCompendium(m_hScriptCompendium, ezResourceAcquireMode::BlockTillLoaded_NeverFail);
  if (pCompendium.GetAcquireResult() != ezResourceAcquireResult::Final)
  {
    return EZ_FAILURE;
  }

  auto itType = pCompendium->GetDescriptor().m_AssetGuidToInfo.Find(typeGuid);
  if (!itType.IsValid())
  {
    return EZ_FAILURE;
  }

  const ezString& sComponentPath = itType.Value().m_sComponentFilePath;
  const ezString& sComponentName = itType.Value().m_sComponentTypeName;

  const ezStringBuilder sCompModule("__", sComponentName);

  m_Duk.PushGlobalObject();

  ezStringBuilder req;
  req.Format("var {} = require(\"./{}\");", sCompModule, itType.Value().m_sComponentFilePath);
  if (m_Duk.ExecuteString(req).Failed())
  {
    ezLog::Error("Could not load component");
    return EZ_FAILURE;
  }

  m_Duk.PopStack();

  m_TsComponentTypes[typeGuid].m_sComponentTypeName = sComponentName;
  RegisterMessageHandlersForComponentType(sComponentName, typeGuid);

  bLoaded = true;

  out_TypeInfo = m_TsComponentTypes.FindOrAdd(typeGuid, nullptr);

  return EZ_SUCCESS;
}

void ezTypeScriptBinding::CleanupStash(ezUInt32 uiNumIterations)
{
  if (!m_LastCleanupObj.IsValid())
    m_LastCleanupObj = m_GameObjectToStashIdx.GetIterator();

  ezDuktapeHelper duk(m_Duk); // [ ]
  duk.PushGlobalStash();      // [ stash ]

  for (ezUInt32 i = 0; i < uiNumIterations && m_LastCleanupObj.IsValid(); ++i)
  {
    ezGameObject* pGO;
    if (!m_pWorld->TryGetObject(m_LastCleanupObj.Key(), pGO))
    {
      duk.PushNull();                                        // [ stash null ]
      duk_put_prop_index(duk, -2, m_LastCleanupObj.Value()); // [ stash ]

      m_FreeStashObjIdx.PushBack(m_LastCleanupObj.Value());
      m_LastCleanupObj = m_GameObjectToStashIdx.Remove(m_LastCleanupObj);
    }
    else
    {
      ++m_LastCleanupObj;
    }
  }

  if (!m_LastCleanupComp.IsValid())
    m_LastCleanupComp = m_ComponentToStashIdx.GetIterator();

  for (ezUInt32 i = 0; i < uiNumIterations && m_LastCleanupComp.IsValid(); ++i)
  {
    ezComponent* pGO;
    if (!m_pWorld->TryGetComponent(m_LastCleanupComp.Key(), pGO))
    {
      duk.PushNull();                                         // [ stash null ]
      duk_put_prop_index(duk, -2, m_LastCleanupComp.Value()); // [ stash ]

      m_FreeStashObjIdx.PushBack(m_LastCleanupComp.Value());
      m_LastCleanupComp = m_ComponentToStashIdx.Remove(m_LastCleanupComp);
    }
    else
    {
      ++m_LastCleanupComp;
    }
  }

  duk.PopStack(); // [ ]

  EZ_DUK_RETURN_VOID_AND_VERIFY_STACK(duk, 0);
}

void ezTypeScriptBinding::StoreReferenceInStash(duk_context* pDuk, ezUInt32 uiStashIdx)
{
  ezDuktapeHelper duk(pDuk);               // [ object ]
  duk.PushGlobalStash();                   // [ object stash ]
  duk_dup(duk, -2);                        // [ object stash object ]
  duk_put_prop_index(duk, -2, uiStashIdx); // [ object stash ]
  duk.PopStack();                          // [ object ]

  EZ_DUK_RETURN_VOID_AND_VERIFY_STACK(duk, 0);
}

bool ezTypeScriptBinding::DukPushStashObject(duk_context* pDuk, ezUInt32 uiStashIdx)
{
  ezDuktapeHelper duk(pDuk);

  duk.PushGlobalStash();          // [ stash ]
  duk_push_uint(duk, uiStashIdx); // [ stash idx ]

  if (!duk_get_prop(duk, -2)) // [ stash obj/undef ]
  {
    duk_pop_2(duk);     // [ ]
    duk_push_null(duk); // [ null ]
    EZ_DUK_RETURN_AND_VERIFY_STACK(duk, false, +1);
  }
  else // [ stash obj ]
  {
    duk_replace(duk, -2); // [ obj ]
    EZ_DUK_RETURN_AND_VERIFY_STACK(duk, true, +1);
  }
}
