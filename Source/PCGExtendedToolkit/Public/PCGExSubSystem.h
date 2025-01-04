// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExDataFilter.h"
#include "Engine/Level.h"
#include "Subsystems/WorldSubsystem.h"
#include "PCGData.h"

#include "PCGExSubSystem.generated.h"

#define PCGEX_SUBSYSTEM UPCGExSubSystem* PCGExSubsystem = UPCGExSubSystem::GetSubsystemForCurrentWorld(); check(PCGExSubsystem)

namespace PCGEx
{
	class FValueMap : public TSharedFromThis<FValueMap>
	{
	protected:
		FRWLock InternalLock;

	public:
		FValueMap() = default;
		~FValueMap() = default;
	};

	template <typename KeyType, typename ValueType>
	class TValueMap : public FValueMap
	{
	protected:
		TMap<KeyType, ValueType> Data;

	public:
		FORCEINLINE void Set(const KeyType At, const ValueType Value)
		{
			FWriteScopeLock WriteScopeLock(InternalLock);
			Data.Add(At, Value);
		}

		FORCEINLINE bool Get(const KeyType At, ValueType& OutValue)
		{
			FReadScopeLock ReadScopeLock(InternalLock);
			ValueType* ValuePtr = Data.Find(At);
			if (ValuePtr)
			{
				OutValue = *ValuePtr;
				return true;
			}
			return false;
		}
	};

	class FDataHandle : public TSharedFromThis<FDataHandle>
	{
	protected:
		FRWLock InternalLock;

	public:
		FDataHandle() = default;
		~FDataHandle() = default;
	};
}

UCLASS()
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExDataHolder : public UObject
{
	GENERATED_BODY()

public:
	TSet<FString> Tags;
	FName Id;
	TWeakPtr<UPCGComponent> SourceComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPCGData> Data = nullptr;
};

UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExSubSystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

	FRWLock TickActionsLock;
	FRWLock GlobalMetadataLock;
	FRWLock GlobalPCGDataLock;

public:
	UPCGExSubSystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** To be used when a PCG component can not have a world anymore, to unregister itself. */
	static UPCGExSubSystem* GetSubsystemForCurrentWorld();

	//~ Begin FTickableGameObject
	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsTickableInEditor() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject

	/** Will return the subsystem from the World if it exists and if it is initialized */
	static UPCGExSubSystem* GetInstance(UWorld* World);

	/** Adds an action that will be executed once at the beginning of this subsystem's next Tick(). */
	using FTickAction = TFunction<void()>;
	void RegisterBeginTickAction(FTickAction&& Action);

#pragma region Global values

	template <typename ValueType>
	bool GetGlobalMetadataEntry(uint32 Key, ValueType& OutValue)
	{
		TSharedPtr<PCGEx::FValueMap>* Map = nullptr;

		{
			FReadScopeLock ReadScopeLock(GlobalMetadataLock);
			Map = PerTypeValueMaps.Find(PCGEx::GetMetadataType<ValueType>());
			if (Map)
			{
				TSharedPtr<PCGEx::TValueMap<uint32, ValueType>> TypedMap = StaticCastSharedPtr<PCGEx::TValueMap<uint32, ValueType>>(*Map);
				return TypedMap->Get(Key, OutValue);
			}
		}

		return false;
	}

	template <typename ValueType>
	void SetGlobalMetadataEntry(uint32 Key, ValueType& OutValue)
	{
		TSharedPtr<PCGEx::FValueMap>* Map = nullptr;

		EPCGMetadataTypes TypeKey = PCGEx::GetMetadataType<ValueType>();
		{
			FReadScopeLock ReadScopeLock(GlobalMetadataLock);
			Map = PerTypeValueMaps.Find(TypeKey);
			if (Map)
			{
				TSharedPtr<PCGEx::TValueMap<uint32, ValueType>> TypedMap = StaticCastSharedPtr<PCGEx::TValueMap<uint32, ValueType>>(*Map);
				TypedMap->Set(Key, OutValue);
			}
		}

		{
			FWriteScopeLock WriteScopeLock(GlobalMetadataLock);
			Map = PerTypeValueMaps.Find(TypeKey);
			if (Map)
			{
				TSharedPtr<PCGEx::TValueMap<uint32, ValueType>> TypedMap = StaticCastSharedPtr<PCGEx::TValueMap<uint32, ValueType>>(*Map);
				TypedMap->Set(Key, OutValue);
			}
			else
			{
				TSharedPtr<PCGEx::TValueMap<uint32, ValueType>> TypedMap = MakeShared<PCGEx::TValueMap<uint32, ValueType>>();
				TypedMap->Set(Key, OutValue);
				PerTypeValueMaps.Add(TypeKey, TypedMap);
			}
		}
	}

	UPROPERTY(Transient)
	TMap<uint32, TObjectPtr<UPCGExDataHolder>> GlobalPCGData;

	void RegisterGlobalData(FName InName, const TObjectPtr<UPCGData>& InData, const TSet<FString>& InTags);
	void UnregisterGlobalData(const TObjectPtr<UPCGData>& InData);
	UPCGExDataHolder* FindGlobalDataByKey(uint32 Key);
	UPCGExDataHolder* FindGlobalDataById(FName InId, EPCGDataType InTypeFilter);
	void FindTaggedGlobalData(const FString& Tagged, const TSet<FString>& NotTagged);

#pragma endregion

protected:
	bool bWantsTick = false;

	/** Functions will be executed at the beginning of the tick and then removed from this array. */
	TArray<FTickAction> BeginTickActions;

	void ExecuteBeginTickActions();

	TMap<EPCGMetadataTypes, TSharedPtr<PCGEx::FValueMap>> PerTypeValueMaps;
};
