#pragma once

#include <Foundation/Basics.h>
#include <RendererFoundation/Basics.h>

// Configure the DLL Import/Export Define
#if EZ_ENABLED(EZ_COMPILE_ENGINE_AS_DLL)
  #ifdef BUILDSYSTEM_BUILDING_RENDERERDX11_LIB
    #define EZ_RENDERERDX11_DLL __declspec(dllexport)
    #define EZ_RENDERERDX11_TEMPLATE
  #else
    #define EZ_RENDERERDX11_DLL __declspec(dllimport)
    #define EZ_RENDERERDX11_TEMPLATE extern
  #endif
#else
  #define EZ_RENDERERDX11_DLL
  #define EZ_RENDERERDX11_TEMPLATE
#endif


#define EZ_GAL_DX11_RELEASE(d3dobj) do { if((d3dobj) != NULL) { (d3dobj)->Release(); (d3dobj) = NULL; } } while(0);
