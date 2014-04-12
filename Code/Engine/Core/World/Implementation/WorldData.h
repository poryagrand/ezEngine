#pragma once

#include <Foundation/Communication/MessageQueue.h>
#include <Foundation/Containers/DynamicArray.h>
#include <Foundation/Containers/HashTable.h>
#include <Foundation/Containers/IdTable.h>
#include <Foundation/Containers/StaticArray.h>

#include <Foundation/Memory/BlockStorage.h>
#include <Foundation/Memory/CommonAllocators.h>
#include <Foundation/Memory/LargeBlockAllocator.h>
#include <Foundation/Profiling/Profiling.h>
#include <Foundation/Strings/String.h>
#include <Foundation/Threading/TaskSystem.h>

#include <Core/World/GameObject.h>

namespace ezInternal
{
  struct WorldData
  {
    WorldData(const char* szWorldName);
    ~WorldData();

    ezString m_Name;
    ezProxyAllocator m_Allocator;
    ezLocalAllocatorWrapper m_AllocatorWrapper;
    ezLargeBlockAllocator m_BlockAllocator;

    enum
    {
      GAME_OBJECTS_PER_BLOCK = ezDataBlock<ezGameObject>::CAPACITY,
      TRANSFORMATION_DATA_PER_BLOCK = ezDataBlock<ezGameObject::TransformationData>::CAPACITY
    };

    // object storage
    typedef ezBlockStorage<ezGameObject> ObjectStorage;
    ezIdTable<ezGameObjectId, ObjectStorage::Entry, ezLocalAllocatorWrapper> m_Objects;
    ObjectStorage m_ObjectStorage;

    ezDynamicArray<ObjectStorage::Entry, ezLocalAllocatorWrapper> m_DeadObjects;

    // hierarchy structures
    struct Hierarchy
    {
      typedef ezDataBlock<ezGameObject::TransformationData> DataBlock;
      typedef ezDynamicArray<DataBlock> DataBlockArray;

      ezHybridArray<DataBlockArray*, 8, ezLocalAllocatorWrapper> m_Data;
    };

    struct HierarchyType
    {
      enum Enum
      {
        Static,
        Dynamic,
        COUNT
      };
    };

    Hierarchy m_Hierarchies[HierarchyType::COUNT];

    ezUInt32 CreateTransformationData(const ezBitflags<ezObjectFlags>& objectFlags, ezUInt32 uiHierarchyLevel,
      ezGameObject::TransformationData*& out_pData);

    void DeleteTransformationData(const ezBitflags<ezObjectFlags>& objectFlags, ezUInt32 uiHierarchyLevel, 
        ezUInt32 uiIndex);

    static void UpdateWorldTransform(ezGameObject::TransformationData* pData);
    static void UpdateWorldTransformWithParent(ezGameObject::TransformationData* pData);

    template <typename UPDATER>
    static void UpdateHierarchyLevel(Hierarchy::DataBlockArray& blocks);

    void UpdateWorldTransforms();

    // game object lookups
    ezHashTable<ezUInt64, ezGameObjectId, ezHashHelper<ezUInt64>, ezLocalAllocatorWrapper> m_PersistentToInternalTable;
    ezHashTable<ezGameObjectId, ezString, ezHashHelper<ezGameObjectId>, ezLocalAllocatorWrapper> m_InternalToNameTable;
    ezHashTable<const char*, ezGameObjectId, ezHashHelper<const char*>, ezLocalAllocatorWrapper> m_NameToInternalTable;

    // component manager
    ezDynamicArray<ezComponentManagerBase*, ezLocalAllocatorWrapper> m_ComponentManagers;

    ezDynamicArray<ezComponentManagerBase::ComponentStorageEntry, ezLocalAllocatorWrapper> m_DeadComponents;

    typedef ezComponentManagerBase::UpdateFunction UpdateFunction;
    struct RegisteredUpdateFunction
    {
      EZ_DECLARE_POD_TYPE();

      UpdateFunction m_Function;
      const char* m_szFunctionName;
      ezUInt32 m_uiGranularity;
    };
  
    struct UpdateTask : public ezTask
    {
      virtual void Execute() EZ_OVERRIDE;

      UpdateFunction m_Function;
      ezUInt32 m_uiStartIndex;
      ezUInt32 m_uiCount;
    };

    ezProfilingId m_UpdateProfilingID;

    ezDynamicArray<RegisteredUpdateFunction, ezLocalAllocatorWrapper> m_UpdateFunctions[ezComponentManagerBase::UpdateFunctionDesc::PHASE_COUNT];
    ezDynamicArray<ezComponentManagerBase::UpdateFunctionDesc, ezLocalAllocatorWrapper> m_UnresolvedUpdateFunctions;

    ezDynamicArray<UpdateTask*, ezLocalAllocatorWrapper> m_UpdateTasks;

    struct QueuedMsgMetaData
    {
      ezGameObjectHandle m_ReceiverObject;
      ezBitflags<ezObjectMsgRouting> m_Routing;
      float m_fDelay;
    };

    /// \todo temp allocator
    typedef ezMessageQueue<QueuedMsgMetaData, ezMutex, ezLocalAllocatorWrapper, ezLocalAllocatorWrapper> MessageQueue;
    MessageQueue m_MessageQueues[ezObjectMsgQueueType::COUNT];

    ezUInt32 m_uiHandledMessageCounter;

    ezThreadID m_ThreadID;
    bool m_bIsInAsyncPhase;

    void* m_pUserData;
  };
}

#include <Core/World/Implementation/WorldData_inl.h>

