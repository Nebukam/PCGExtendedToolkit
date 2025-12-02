// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
//#include "CollisionQueryParams.h"
//#include "Engine/HitResult.h"

#include "PCGContext.h"
#include "PCGExCommon.h"

class UPCGExInstancedFactory;
class UPCGComponent;
class IPCGExElement;
class UPCGManagedComponent;
struct FStreamableHandle;
struct FAttachmentTransformRules;

namespace PCGExMT
{
	class FTaskManager;
}

namespace PCGEx
{
	class FUniqueNameGenerator;
	class FManagedObjects;
	class FWorkHandle;
}

struct PCGEXTENDEDTOOLKIT_API FPCGExContext : FPCGContext
{
	friend class IPCGExElement;

protected:
	
	enum class EExecutionPolicy : int8
	{
		Normal,
		AsyncEx,
		AsyncTask
	};
	
	mutable FRWLock AsyncLock;
	mutable FRWLock StagedOutputLock;
	mutable FRWLock AssetDependenciesLock;

	TSharedPtr<PCGEx::FWorkHandle> WorkHandle;
	const IPCGExElement* ElementHandle = nullptr;

public:
	TWeakPtr<PCGEx::FWorkHandle> GetWorkHandle() { return WorkHandle; }
	TSharedPtr<PCGEx::FManagedObjects> ManagedObjects;
	EPCGExAsyncPriority WorkPriority = EPCGExAsyncPriority::Default;
	EExecutionPolicy ExecutionPolicy = EExecutionPolicy::Normal;

	// TODO : bool toggle for hoarder execution 
	
	bool bScopedAttributeGet = false;
	bool bPropagateAbortedExecution = false;

	FPCGExContext();

	virtual ~FPCGExContext() override;

	UPCGExInstancedFactory* RegisterOperation(UPCGExInstancedFactory* BaseOperation, const FName OverridePinLabel = NAME_None);

#pragma region Output Data

	bool bFlattenOutput = false;

	void IncreaseStagedOutputReserve(const int32 InIncreaseNum);
	FPCGTaggedData& StageOutput(UPCGData* InData, const bool bManaged, const bool bIsMutable);
	void StageOutput(UPCGData* InData, const FName& InPin, const TSet<FString>& InTags, const bool bManaged, const bool bIsMutable, const bool bPinless);
	FPCGTaggedData& StageOutput(UPCGData* InData, const bool bManaged);

#pragma endregion

	UWorld* GetWorld() const;
	const UPCGComponent* GetComponent() const;
	UPCGComponent* GetMutableComponent() const;

#pragma region State

	TSharedPtr<PCGExMT::FTaskManager> GetAsyncManager();

	void PauseContext();
	void UnpauseContext();

	void SetState(const PCGExCommon::ContextState StateId);
	void SetAsyncState(const PCGExCommon::ContextState WaitState);

	virtual bool IsWaitingForTasks();
	void ReadyForExecution();

	bool IsState(const PCGExCommon::ContextState StateId) const { return CurrentState.load(std::memory_order_acquire) == StateId; }
	bool IsInitialExecution() const { return IsState(PCGExCommon::State_InitialExecution); }
	bool IsDone() const { return IsState(PCGExCommon::State_Done); }
	bool IsWorkCompleted() const { return bWorkCompleted.load(std::memory_order_acquire); }
	bool IsWorkCancelled() const { return bWorkCancelled.load(std::memory_order_acquire); }
	void Done();

	bool TryComplete(const bool bForce = false);

	virtual void ResumeExecution();

protected:
	std::atomic<PCGExCommon::ContextState> CurrentState;
	std::atomic<bool> bWorkCompleted{false};
	std::atomic<bool> bWorkCancelled{false};

	TSharedPtr<PCGExMT::FTaskManager> AsyncManager;

	virtual void OnComplete();

#pragma endregion

#pragma region Async resource management

public:
	void CancelAssetLoading();

	TSet<FSoftObjectPath>& GetRequiredAssets();
	bool HasAssetRequirements() const { return RequiredAssets && !RequiredAssets->IsEmpty(); }

	virtual void RegisterAssetDependencies();
	void AddAssetDependency(const FSoftObjectPath& Dependency);
	void LoadAssets();

protected:
	bool bForceSynchronousAssetLoad = false;
	bool bAssetLoadRequested = false;
	bool bAssetLoadError = false;
	TSharedPtr<TSet<FSoftObjectPath>> RequiredAssets;

	/** Handle holder for any loaded resources */
	TSharedPtr<FStreamableHandle> LoadHandle;

#pragma endregion

#pragma region Managed Components

public:
	UPCGManagedComponent* AttachManagedComponent(AActor* InParent, UActorComponent* InComponent, const FAttachmentTransformRules& AttachmentRules) const;

#pragma endregion

protected:
	TSet<FName> ConsumableAttributesSet;
	TSet<FName> ProtectedAttributesSet;

	mutable FRWLock ConsumableAttributesLock;
	mutable FRWLock ProtectedAttributesLock;

public:
	bool bCleanupConsumableAttributes = false;
	void AddConsumableAttributeName(FName InName);
	void AddProtectedAttributeName(FName InName);

	TSharedPtr<PCGEx::FUniqueNameGenerator> UniqueNameGenerator;

	void EDITOR_TrackPath(const FSoftObjectPath& Path, bool bIsCulled = false);
	void EDITOR_TrackClass(const TSubclassOf<UObject>& InSelectionClass, bool bIsCulled = false);

	bool CanExecute() const;
	bool bQuietInvalidInputWarning = false;

	bool bQuietMissingAttributeError = false;
	bool bQuietMissingInputError = false;
	bool bQuietCancellationError = false;

	virtual bool CancelExecution(const FString& InReason);

protected:
	mutable FRWLock NotifyActorsLock;

	// Actors to notify when execution is complete
	TSet<AActor*> NotifyActors;

	void ExecuteOnNotifyActors(const TArray<FName>& FunctionNames);
	virtual void AddExtraStructReferencedObjects(FReferenceCollector& Collector) override;

	TArray<UPCGExInstancedFactory*> ProcessorOperations;
	TSet<UPCGExInstancedFactory*> InternalOperations;

public:
	void AddNotifyActor(AActor* InActor);
};
