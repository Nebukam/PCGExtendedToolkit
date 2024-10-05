// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Runtime/Launch/Resources/Version.h"

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGExHelpers.h"
#include "Engine/StreamableManager.h"

namespace PCGEx
{
	using AsyncState = uint64;

#define PCGEX_ASYNC_STATE(_NAME) const PCGEx::AsyncState _NAME = GetTypeHash(FName(#_NAME));

	PCGEX_ASYNC_STATE(State_Preparation)
	PCGEX_ASYNC_STATE(State_LoadingAssetDependencies)
	PCGEX_ASYNC_STATE(State_AsyncPreparation)

	PCGEX_ASYNC_STATE(State_InitialExecution)
	PCGEX_ASYNC_STATE(State_ReadyForNextPoints)
	PCGEX_ASYNC_STATE(State_ProcessingPoints)

	PCGEX_ASYNC_STATE(State_WaitingOnAsyncWork)
	PCGEX_ASYNC_STATE(State_Done)

	PCGEX_ASYNC_STATE(State_Processing)
	PCGEX_ASYNC_STATE(State_Completing)
	PCGEX_ASYNC_STATE(State_Writing)

	PCGEX_ASYNC_STATE(State_UnionWriting)
}

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExContext : FPCGContext
{
protected:
	mutable FRWLock StagedOutputLock;

	TArray<FPCGTaggedData> StagedOutputs;
	bool bFlattenOutput = false;

	int32 LastReserve = 0;
	int32 AdditionsSinceLastReserve = 0;

	void CommitStagedOutputs();

public:
	TSharedPtr<PCGEx::FManagedObjects> ManagedObjects;

	FPCGExContext();

	virtual ~FPCGExContext() override;

	void StagedOutputReserve(const int32 NumAdditions);

	void StageOutput(const FName Pin, UPCGData* InData, const TSet<FString>& InTags, bool bManaged);
	void StageOutput(const FName Pin, UPCGData* InData, bool bManaged);

#pragma region State

	void PauseContext();
	void UnpauseContext();

	bool bAsyncEnabled = true;
	void SetState(const PCGEx::AsyncState StateId);
	void SetAsyncState(const PCGEx::AsyncState WaitState);

	virtual bool ShouldWaitForAsync();
	void ReadyForExecution();

	bool IsState(const PCGEx::AsyncState StateId) const { return CurrentState == StateId; }
	bool IsInitialExecution() const { return IsState(PCGEx::State_InitialExecution); }
	bool IsDone() const { return IsState(PCGEx::State_Done); }
	void Done();

	virtual void OnComplete();
	bool TryComplete(const bool bForce = false);

protected:
	bool bWaitingForAsyncCompletion = false;
	virtual void ResumeExecution();
	PCGEx::AsyncState CurrentState;

#pragma endregion

#pragma region Async resource management

public:
	void CancelAssetLoading();

	bool HasAssetRequirements() const { return !RequiredAssets.IsEmpty(); }

	virtual void RegisterAssetDependencies();
	void RegisterAssetRequirement(const FSoftObjectPath& Dependency);
	void LoadAssets();

	TSet<FSoftObjectPath>& GetRequiredAssets() { return RequiredAssets; }

protected:
	bool bForceSynchronousAssetLoad = false;
	bool bAssetLoadRequested = false;
	bool bAssetLoadError = false;
	TSet<FSoftObjectPath> RequiredAssets;

	/** Handle holder for any loaded resources */
	TSharedPtr<FStreamableHandle> LoadHandle;

#pragma endregion

public:
	bool CanExecute() const;
	bool CancelExecution(const FString& InReason);

protected:
	bool bExecutionCancelled = false;
};
