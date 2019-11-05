#include <EditorPluginTypeScriptPCH.h>

#include <EditorFramework/EditorApp/EditorApp.moc.h>
#include <EditorPluginTypeScript/TypeScriptAsset/TypeScriptAssetObjects.h>

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezTypeScriptAssetProperties, 1, ezRTTIDefaultAllocator<ezTypeScriptAssetProperties>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("ComponentName", m_sComponentName),
    EZ_MEMBER_PROPERTY("ScriptFile", m_sScriptFile)->AddAttributes(new ezFileBrowserAttribute("Select Script", "*.ts")),
  }
  EZ_END_PROPERTIES;
}
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

ezTypeScriptAssetProperties::ezTypeScriptAssetProperties() = default;
ezTypeScriptAssetProperties::~ezTypeScriptAssetProperties() = default;