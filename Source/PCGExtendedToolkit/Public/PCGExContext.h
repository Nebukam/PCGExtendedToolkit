// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGExHelpers.h"
#include "PCGManagedResource.h"
#include "Engine/StreamableManager.h"

namespace PCGEx
{
	using ContextState = uint64;

#define PCGEX_CTX_STATE(_NAME) const PCGEx::ContextState _NAME = GetTypeHash(FName(#_NAME));

	PCGEX_CTX_STATE(State_Preparation)
	PCGEX_CTX_STATE(State_LoadingAssetDependencies)
	PCGEX_CTX_STATE(State_AsyncPreparation)
	PCGEX_CTX_STATE(State_FacadePreloading)

	PCGEX_CTX_STATE(State_InitialExecution)
	PCGEX_CTX_STATE(State_ReadyForNextPoints)
	PCGEX_CTX_STATE(State_ProcessingPoints)

	PCGEX_CTX_STATE(State_WaitingOnAsyncWork)
	PCGEX_CTX_STATE(State_Done)

	PCGEX_CTX_STATE(State_Processing)
	PCGEX_CTX_STATE(State_Completing)
	PCGEX_CTX_STATE(State_Writing)

	PCGEX_CTX_STATE(State_UnionWriting)
}

#if PCGEX_ENGINE_VERSION < 505
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGContextHandle : public TSharedFromThis<FPCGContextHandle>
{
public:
	FPCGContextHandle(FPCGContext* InContext)
		: Context(InContext)
	{
	}

	FPCGContext* GetContext() { return Context; }

private:
	FPCGContext* Context = nullptr;
};
#endif

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExContext : FPCGContext
{
protected:
	mutable FRWLock StagedOutputLock;
	mutable FRWLock AssetDependenciesLock;

	TArray<FPCGTaggedData> StagedOutputs;
	bool bFlattenOutput = false;

	int32 LastReserve = 0;
	int32 AdditionsSinceLastReserve = 0;
	TSet<FName> ConsumableAttributesSet;
	TSet<FName> ProtectedAttributesSet;

	void CommitStagedOutputs();

public:
	TSharedPtr<PCGEx::FLifecycle> Lifecycle;
	TUniquePtr<PCGEx::FManagedObjects> ManagedObjects;

	bool bScopedAttributeGet = false;

	FPCGExContext();

	virtual ~FPCGExContext() override;

	void StagedOutputReserve(const int32 NumAdditions);

	void StageOutput(const FName Pin, UPCGData* InData, const TSet<FString>& InTags, bool bManaged, bool bIsMutable);
	void StageOutput(const FName Pin, UPCGData* InData, bool bManaged);

#pragma region State

	void PauseContext();
	void UnpauseContext();

	bool bAsyncEnabled = true;
	void SetState(const PCGEx::ContextState StateId);
	void SetAsyncState(const PCGEx::ContextState WaitState);

	virtual bool ShouldWaitForAsync();
	void ReadyForExecution();

	bool IsState(const PCGEx::ContextState StateId) const { return CurrentState == StateId; }
	bool IsInitialExecution() const { return IsState(PCGEx::State_InitialExecution); }
	bool IsDone() const { return IsState(PCGEx::State_Done); }
	void Done();

	virtual void OnComplete();
	bool TryComplete(const bool bForce = false);

	virtual void ResumeExecution();

protected:
	bool bWaitingForAsyncCompletion = false;
	PCGEx::ContextState CurrentState;

#pragma endregion

#if PCGEX_ENGINE_VERSION < 505

	// Lazy initialized shared handle pointer that can be used in lambda captures to test if Context is still valid before accessing it
	TSharedPtr<FPCGContextHandle> Handle;

public:
	TWeakPtr<FPCGContextHandle> GetOrCreateHandle()
	{
		if (!Handle) { Handle = MakeShared<FPCGContextHandle>(this); }
		return Handle.ToWeakPtr();
	}

	template <typename T>
	static T* GetContextFromHandle(const TWeakPtr<FPCGContextHandle> WeakHandle)
	{
		if (const TSharedPtr<FPCGContextHandle> SharedHandle = WeakHandle.Pin()) { return static_cast<T*>(SharedHandle->GetContext()); }
		return nullptr;
	}

#endif

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
	UPCGManagedComponent* AttachManagedComponent(AActor* InParent, USceneComponent* InComponent, const FAttachmentTransformRules& AttachmentRules) const;

#pragma endregion

	mutable FRWLock ConsumableAttributesLock;
	mutable FRWLock ProtectedAttributesLock;
	bool bCleanupConsumableAttributes = false;
	TSet<FName>& GetConsumableAttributesSet() { return ConsumableAttributesSet; }
	void AddConsumableAttributeName(FName InName);
	void AddProtectedAttributeName(FName InName);

	bool CanExecute() const;
	virtual bool CancelExecution(const FString& InReason);

protected:
	bool bExecutionCancelled = false;
};
