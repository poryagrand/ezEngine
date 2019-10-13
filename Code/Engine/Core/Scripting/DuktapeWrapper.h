#pragma once

#include <Core/CoreDLL.h>
#include <Core/Scripting/DuktapeHelper.h>
#include <Foundation/Logging/Log.h>
#include <Foundation/Memory/CommonAllocators.h>
#include <Foundation/Strings/String.h>

#ifdef BUILDSYSTEM_ENABLE_DUKTAPE_SUPPORT

struct duk_hthread;
typedef duk_hthread duk_context;
typedef int (*duk_c_function)(duk_context* ctx);


class EZ_CORE_DLL ezDuktapeWrapper : public ezDuktapeHelper
{
  EZ_DISALLOW_COPY_AND_ASSIGN(ezDuktapeWrapper);

public:
  ezDuktapeWrapper(const char* szWrapperName);
  ezDuktapeWrapper(duk_context* pExistingContext);
  ~ezDuktapeWrapper();

  /// \name Basics
  ///@{

  /// \brief Enables support for loading modules via the 'require' function
  void EnableModuleSupport(duk_c_function pModuleSearchFunction);

  ///@}

  /// \name Executing Scripts
  ///@{

  ezResult ExecuteString(const char* szString, const char* szDebugName = "eval");

  ezResult ExecuteStream(ezStreamReader& stream, const char* szDebugName);

  ezResult ExecuteFile(const char* szFile);

  ///@}

  /// \name C Functions
  ///@{

  ezResult BeginFunctionCall(const char* szFunctionName, bool bForceLocalObject = false);
  ezResult BeginMethodCall(const char* szMethodName);
  ezResult ExecuteFunctionCall();
  ezResult ExecuteMethodCall();
  void EndFunctionCall();
  void EndMethodCall();

  ///@}
  /// \name Working with Objects
  ///@{

  ezResult OpenObject(const char* szObjectName);
  void OpenGlobalObject();
  void OpenGlobalStashObject();
  void CloseObject();

  ///@}

private:
  void InitializeContext();
  void DestroyContext();

  static void FatalErrorHandler(void* pUserData, const char* szMsg);
  static void* DukAlloc(void* pUserData, size_t size);
  static void* DukRealloc(void* pUserData, void* pPointer, size_t size);
  static void DukFree(void* pUserData, void* pPointer);


protected:
  struct States
  {
    ezInt32 m_iOpenObjects = 0;
    bool m_bAutoOpenedGlobalObject = false;
  };

  bool m_bIsInFunctionCall = false;
  bool m_bInitializedModuleSupport = false;
  States m_States;

private:
  /// If this script created the context, it also releases it on exit.
  bool m_bReleaseOnExit = true;

#  if EZ_ENABLED(EZ_COMPILE_FOR_DEBUG)
  ezAllocator<ezMemoryPolicies::ezHeapAllocation, ezMemoryTrackingFlags::EnableTracking> m_Allocator;
#  else
  ezAllocator<ezMemoryPolicies::ezHeapAllocation, ezMemoryTrackingFlags::None> m_Allocator;
#  endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class EZ_CORE_DLL ezDuktapeFunction final : public ezDuktapeWrapper
{
public:
  ezDuktapeFunction(duk_context* pExistingContext);
  ~ezDuktapeFunction();

  /// \name Retrieving function parameters
  ///@{

  /// Returns how many Parameters were passed to the called C-Function.
  ezUInt32 GetNumVarArgFunctionParameters() const;

  ezInt16 GetFunctionMagicValue() const;

  ///@}

  /// \name Returning values from C function
  ///@{

  ezInt32 ReturnVoid();
  ezInt32 ReturnNull();
  ezInt32 ReturnUndefined();
  ezInt32 ReturnBool(bool value);
  ezInt32 ReturnInt(ezInt32 value);
  ezInt32 ReturnUInt(ezUInt32 value);
  ezInt32 ReturnFloat(float value);
  ezInt32 ReturnNumber(double value);
  ezInt32 ReturnString(const char* value);
  ezInt32 ReturnCustom();

  ///@}

private:
  bool m_bDidReturnValue = false;
};

//////////////////////////////////////////////////////////////////////////

class EZ_CORE_DLL ezDuktapeStackValidator
{
public:
  ezDuktapeStackValidator(duk_context* pContext, ezInt32 iExpectedChange = 0);
  ~ezDuktapeStackValidator();

  void AdjustExpected(ezInt32 iChange);

private:
  duk_context* m_pContext = nullptr;
  ezInt32 m_iStackTop = 0;
};

#  include <Core/Scripting/DuktapeWrapper/DuktapeWrapper.inl>

#endif // BUILDSYSTEM_ENABLE_DUKTAPE_SUPPORT
