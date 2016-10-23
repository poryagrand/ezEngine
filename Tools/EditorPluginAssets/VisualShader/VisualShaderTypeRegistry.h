#pragma once

#include <Foundation/Basics.h>
#include <Foundation/Configuration/Singleton.h>
#include <Foundation/Configuration/Startup.h>
#include <Foundation/Strings/String.h>
#include <ToolsFoundation/NodeObject/DocumentNodeManager.h>

class ezVisualShaderPinDescriptor
{
public:
  ezString m_sName;
  const ezRTTI* m_pDataType;
  ezReflectedPropertyDescriptor m_PropertyDesc;
  ezColorGammaUB m_Color;
  ezVariant m_DefaultValue;
};

struct ezVisualShaderNodeType
{
  typedef ezUInt8 StorageType;

  enum Enum
  {
    Generic,
    Main,

    Default = Generic
  };
};

class ezVisualShaderNodeDescriptor
{
public:

  ezEnum<ezVisualShaderNodeType> m_NodeType;
  ezString m_sName;
  ezColorGammaUB m_Color;
  ezString m_sShaderCodePixelDefines;
  ezString m_sShaderCodePixelIncludes;
  ezString m_sShaderCodePixelConstants;
  ezString m_sShaderCodePixelBody;
  ezString m_sShaderCodePermutations;
  ezString m_sShaderCodeMaterialParams;
  ezString m_sShaderCodeRenderState;
  ezString m_sShaderCodeVertexShader;

  ezHybridArray<ezVisualShaderPinDescriptor, 4> m_InputPins;
  ezHybridArray<ezVisualShaderPinDescriptor, 4> m_OutputPins;
  ezHybridArray<ezReflectedPropertyDescriptor, 4> m_Properties;
};


class ezVisualShaderTypeRegistry
{
  EZ_DECLARE_SINGLETON(ezVisualShaderTypeRegistry);

public:
  ezVisualShaderTypeRegistry();

  const ezVisualShaderNodeDescriptor* GetDescriptorForType(const ezRTTI* pRtti) const;

  const ezRTTI* GetNodeBaseType() const { return m_pBaseType; }

private:
  EZ_MAKE_SUBSYSTEM_STARTUP_FRIEND(EditorFramework, VisualShader);

  void LoadNodeData();
  const ezRTTI* GenerateTypeFromDesc(const ezVisualShaderNodeDescriptor& desc) const;

  ezMap<const ezRTTI*, ezVisualShaderNodeDescriptor> m_NodeDescriptors;

  const ezRTTI* m_pBaseType;
};
