#include <ParticlePlugin/PCH.h>
#include <ParticlePlugin/System/ParticleSystemInstance.h>
#include <CoreUtils/DataProcessing/Stream/StreamElementSpawner.h>
#include <CoreUtils/DataProcessing/Stream/StreamProcessor.h>
#include <CoreUtils/DataProcessing/Stream/StreamElementIterator.h>
#include <GameUtils/Interfaces/PhysicsWorldModule.h>
#include <Core/World/World.h>
#include <ParticlePlugin/Emitter/ParticleEmitter.h>
#include <ParticlePlugin/Behavior/ParticleBehavior.h>
#include <ParticlePlugin/System/ParticleSystemDescriptor.h>
#include <ParticlePlugin/Initializer/ParticleInitializer.h>
#include <CoreUtils/DataProcessing/Stream/DefaultImplementations/ElementSpawner.h>


ezParticleSystemInstance::ezParticleSystemInstance()
{
  m_bVisible = true;
  m_bEmitterEnabled = true;
}

bool ezParticleSystemInstance::HasActiveParticles() const
{
  return m_StreamGroup.GetNumActiveElements() > 0;
}

void ezParticleSystemInstance::ConfigureFromTemplate(const ezParticleSystemDescriptor* pTemplate)
{
  EZ_LOCK(m_Mutex);

  m_bVisible = pTemplate->m_bVisible;

  bool allSpawnersEqual = true;
  bool allProcessorsEqual = true;

  for (auto& info : m_StreamInfo)
  {
    info.m_bGetsInitialized = false;
    info.m_bInUse = false;
  }

  // emitters
  if (allSpawnersEqual)
  {
    const auto& factories = pTemplate->GetEmitterFactories();

    if (factories.GetCount() == m_Emitters.GetCount())
    {
      for (ezUInt32 i = 0; i < factories.GetCount(); ++i)
      {
        if (factories[i]->GetEmitterType() != m_Emitters[i]->GetDynamicRTTI())
        {
          allSpawnersEqual = false;
          break;
        }
      }
    }
    else
    {
      allSpawnersEqual = false;
    }
  }

  // initializers
  if (allSpawnersEqual)
  {
    const auto& factories = pTemplate->GetInitializerFactories();

    if (factories.GetCount() == m_Initializers.GetCount())
    {
      for (ezUInt32 i = 0; i < factories.GetCount(); ++i)
      {
        if (factories[i]->GetInitializerType() != m_Initializers[i]->GetDynamicRTTI())
        {
          allSpawnersEqual = false;
          break;
        }
      }
    }
    else
    {
      allSpawnersEqual = false;
    }
  }

  // behaviors
  if (allProcessorsEqual)
  {
    const auto& factories = pTemplate->GetBehaviorFactories();

    if (factories.GetCount() == m_Behaviors.GetCount())
    {
      for (ezUInt32 i = 0; i < factories.GetCount(); ++i)
      {
        if (factories[i]->GetBehaviorType() != m_Behaviors[i]->GetDynamicRTTI())
        {
          allProcessorsEqual = false;
          break;
        }
      }
    }
    else
    {
      allProcessorsEqual = false;
    }
  }

  if (!allSpawnersEqual)
  {
    // all spawners get cleared, so clear this as well
    // this has to be done before any streams get created
    for (auto& info : m_StreamInfo)
    {
      info.m_pInitializer = nullptr;
    }
  }

  if (!allSpawnersEqual)
  {
    // recreate emitters, initializers and behaviors
    m_StreamGroup.ClearStreamElementSpawners();

    // emitters
    {
      m_Emitters.Clear();

      for (const auto pFactory : pTemplate->GetEmitterFactories())
      {
        ezParticleEmitter* pEmitter = pFactory->CreateEmitter(this);
        m_StreamGroup.AddStreamElementSpawner(pEmitter);
        m_Emitters.PushBack(pEmitter);
      }
    }

    // initializers
    {
      m_Initializers.Clear();

      for (const auto pFactory : pTemplate->GetInitializerFactories())
      {
        ezParticleInitializer* pInitializer = pFactory->CreateInitializer(this);
        m_StreamGroup.AddStreamElementSpawner(pInitializer);
        m_Initializers.PushBack(pInitializer);
      }
    }
  }
  else
  {
    // just re-initialize the emitters with the new properties

    // emitters
    {
      const auto& factories = pTemplate->GetEmitterFactories();

      for (ezUInt32 i = 0; i < factories.GetCount(); ++i)
      {
        factories[i]->CopyEmitterProperties(m_Emitters[i]);
        m_Emitters[i]->AfterPropertiesConfigured();
        m_Emitters[i]->CreateRequiredStreams();
      }
    }

    // initializers
    {
      const auto& factories = pTemplate->GetInitializerFactories();

      for (ezUInt32 i = 0; i < factories.GetCount(); ++i)
      {
        factories[i]->CopyInitializerProperties(m_Initializers[i]);
        m_Initializers[i]->AfterPropertiesConfigured();
        m_Initializers[i]->CreateRequiredStreams();
      }
    }
  }

  if (!allProcessorsEqual)
  {
    // behaviors
    {
      m_Behaviors.Clear();
      m_StreamGroup.ClearStreamProcessors();

      for (const auto pFactory : pTemplate->GetBehaviorFactories())
      {
        ezParticleBehavior* pBehavior = pFactory->CreateBehavior(this);
        m_StreamGroup.AddStreamProcessor(pBehavior);
        m_Behaviors.PushBack(pBehavior);
      }
    }
  }
  else
  {
    // behaviors
    {
      const auto& factories = pTemplate->GetBehaviorFactories();

      for (ezUInt32 i = 0; i < factories.GetCount(); ++i)
      {
        factories[i]->CopyBehaviorProperties(m_Behaviors[i]);
        m_Behaviors[i]->AfterPropertiesConfigured();
        m_Behaviors[i]->CreateRequiredStreams();
      }
    }
  }

  // setup stream initializers for all streams that have none yet
  CreateStreamZeroInitializers();
}

void ezParticleSystemInstance::Initialize(ezUInt32 uiMaxParticles, ezWorld* pWorld, ezUInt64 uiRandomSeed)
{
  EZ_LOCK(m_Mutex);

  if (uiRandomSeed == 0)
  {
    m_Random.InitializeFromCurrentTime();
  }
  else
  {
    m_Random.Initialize(uiRandomSeed);
  }

  m_bEmitterEnabled = true;
  m_pWorld = pWorld;

  m_StreamGroup.Clear();
  m_Emitters.Clear();
  m_Initializers.Clear();
  m_Behaviors.Clear();

  m_StreamInfo.Clear();
  m_StreamGroup.SetSize(uiMaxParticles);
}

ezParticleSystemState::Enum ezParticleSystemInstance::Update(const ezTime& tDiff)
{
  EZ_LOCK(m_Mutex);

  if (m_bEmitterEnabled)
  {
    // if all emitters are finished, we deactivate this on the whole system
    m_bEmitterEnabled = false;

    for (auto pEmitter : m_Emitters)
    {
      if (!pEmitter->IsFinished())
      {
        m_bEmitterEnabled = true;
        const ezUInt32 uiSpawn = pEmitter->ComputeSpawnCount(tDiff);

        if (uiSpawn > 0)
        {
          m_StreamGroup.SpawnElements(uiSpawn);
        }
      }
    }
  }

  for (auto pBehavior : m_Behaviors)
  {
    pBehavior->StepParticleSystem(tDiff);
  }

  m_StreamGroup.Process();

  if (m_bEmitterEnabled)
    return ezParticleSystemState::Active;

  // all emitters are done, but some particles are still alive
  if (HasActiveParticles())
    return ezParticleSystemState::EmittersFinished;

  return ezParticleSystemState::Inactive;
}

const ezStream* ezParticleSystemInstance::QueryStream(const char* szName, ezStream::DataType Type) const
{
  ezStringBuilder fullName(szName);
  fullName.AppendFormat("(%i)", (int)Type);

  return m_StreamGroup.GetStreamByName(fullName);
}

void ezParticleSystemInstance::CreateStream(const char* szName, ezStream::DataType Type, ezStream** ppStream, ezParticleStreamBinding& binding, bool bExpectInitializedValue)
{
  EZ_ASSERT_DEV(ppStream != nullptr, "The pointer to the stream pointer must not be null");

  ezStringBuilder fullName(szName);
  fullName.AppendFormat("(%i)", (int)Type);

  StreamInfo* pInfo = nullptr;

  ezStream* pStream = m_StreamGroup.GetStreamByName(fullName);
  if (pStream == nullptr)
  {
    pStream = m_StreamGroup.AddStream(fullName, Type);

    pInfo = &m_StreamInfo.ExpandAndGetRef();
    pInfo->m_sName = fullName;
  }
  else
  {
    for (auto& info : m_StreamInfo)
    {
      if (info.m_sName == fullName)
      {
        pInfo = &info;
        break;
      }
    }

    EZ_ASSERT_DEV(pInfo != nullptr, "Could not find info for stream");
  }

  pInfo->m_bInUse = true;
  if (!bExpectInitializedValue)
    pInfo->m_bGetsInitialized = true;

  EZ_ASSERT_DEV(pStream != nullptr, "Stream creation failed ('%s' -> '%s')", szName, fullName.GetData());
  *ppStream = pStream;

  auto& bind = binding.m_Bindings.ExpandAndGetRef();
  bind.m_ppStream = ppStream;
  bind.m_sName = fullName;
}


void ezParticleSystemInstance::CreateStreamZeroInitializers()
{
  for (ezUInt32 i = 0; i < m_StreamInfo.GetCount(); )
  {
    auto& info = m_StreamInfo[i];

    if ((!info.m_bInUse || info.m_bGetsInitialized) && info.m_pInitializer)
    {
      m_StreamGroup.RemoveStreamElementSpawner(info.m_pInitializer);
      info.m_pInitializer = nullptr;
    }

    if (!info.m_bInUse)
    {
      m_StreamGroup.RemoveStreamByName(info.m_sName.GetData());
      m_StreamInfo.RemoveAtSwap(i);
    }
    else
    {
      ++i;
    }
  }

  for (auto& info : m_StreamInfo)
  {
    if (info.m_bGetsInitialized)
      continue;

    EZ_ASSERT_DEV(info.m_bInUse, "Invalid state");

    if (info.m_pInitializer == nullptr)
    {
      info.m_pInitializer = EZ_DEFAULT_NEW(ezStreamElementSpawnerZeroInitialized, info.m_sName);
      m_StreamGroup.AddStreamElementSpawner(info.m_pInitializer);
    }
  }
}

void ezParticleStreamBinding::UpdateBindings(const ezStreamGroup* pGroup) const
{
  for (const auto& bind : m_Bindings)
  {
    ezStream* pStream = pGroup->GetStreamByName(bind.m_sName);
    EZ_ASSERT_DEV(pStream != nullptr, "Stream binding '%s' is invalid now", bind.m_sName.GetData());

    *bind.m_ppStream = pStream;
  }
}

ezParticleSystemInstance::StreamInfo::StreamInfo()
{
  m_pInitializer = nullptr;
  m_bGetsInitialized = false;
  m_bInUse = false;
}
