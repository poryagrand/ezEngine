#pragma once

#include <EditorFramework/Assets/SimpleAssetDocument.h>
#include <VisualShader/VisualShaderNodeManager.h>

class ezMaterialAssetDocument;

struct ezMaterialShaderMode
{
  typedef ezUInt8 StorageType;

  enum Enum
  {
    File,
    Custom,

    Default = File
  };
};

EZ_DECLARE_REFLECTABLE_TYPE(EZ_NO_LINKAGE, ezMaterialShaderMode);

class ezMaterialAssetProperties : public ezReflectedClass
{
  EZ_ADD_DYNAMIC_REFLECTION(ezMaterialAssetProperties, ezReflectedClass);

public:
  ezMaterialAssetProperties() : m_pDocument(nullptr) {}

  void SetBaseMaterial(const char* szBaseMaterial);
  const char* GetBaseMaterial() const;
  void SetShader(const char* szShader);
  const char* GetShader() const;
  void SetShaderProperties(ezReflectedClass* pProperties);
  ezReflectedClass* GetShaderProperties() const;
  void SetShaderMode(ezEnum<ezMaterialShaderMode> mode);
  ezEnum<ezMaterialShaderMode> GetShaderMode() const { return m_ShaderMode; }

  void SetDocument(ezMaterialAssetDocument* pDocument);
  void UpdateShader(bool bForce = false);

  void DeleteProperties();
  void CreateProperties(const char* szShaderPath, bool bForce = false);

  void SaveOldValues();
  void LoadOldValues();
  const ezRTTI* UpdateShaderType(const char* szShaderPath);

  ezString GetFinalShader() const;
  ezString GetAutoGenShaderPathAbs() const;

public:
  ezString m_sBaseMaterial;
  ezString m_sShader;

  ezMap<ezString, ezVariant> m_CachedProperties;
  ezMaterialAssetDocument* m_pDocument;
  ezEnum<ezMaterialShaderMode> m_ShaderMode;
};

class ezMaterialAssetDocument : public ezSimpleAssetDocument<ezMaterialAssetProperties>
{
  EZ_ADD_DYNAMIC_REFLECTION(ezMaterialAssetDocument, ezSimpleAssetDocument<ezMaterialAssetProperties>);

public:
  ezMaterialAssetDocument(const char* szDocumentPath);

  virtual const char* QueryAssetType() const override { return "Material"; }

  ezDocumentObject* GetShaderPropertyObject();
  const ezDocumentObject* GetShaderPropertyObject() const;

  void SetBaseMaterial(const char* szBaseMaterial);

  ezStatus WriteMaterialAsset(ezStreamWriter& stream, const char* szPlatform) const;

protected:
  ezUuid GetSeedFromBaseMaterial(const char* szBaseGraph);
  static ezUuid GetMaterialNodeGuid(const ezAbstractObjectGraph& graph);
  virtual void UpdatePrefabObject(ezDocumentObject* pObject, const ezUuid& PrefabAsset, const ezUuid& PrefabSeed, const char* szBasePrefab) override;
  virtual void InitializeAfterLoading() override;

  virtual ezStatus InternalTransformAsset(ezStreamWriter& stream, const char* szPlatform, const ezAssetFileHeader& AssetHeader) override;
  virtual ezStatus InternalRetrieveAssetInfo(const char* szPlatform) override { return ezStatus(EZ_SUCCESS); }
  virtual ezStatus InternalCreateThumbnail(const ezAssetFileHeader& AssetHeader) override;

  virtual void InternalGetMetaDataHash(const ezDocumentObject* pObject, ezUInt64& inout_uiHash) const override;
  virtual void AttachMetaDataBeforeSaving(ezAbstractObjectGraph& graph) const override;
  virtual void RestoreMetaDataAfterLoading(const ezAbstractObjectGraph& graph) override;
};

class ezMaterialObjectManager : public ezVisualShaderNodeManager
{
};
