// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Runtime/Launch/Resources/Version.h"

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGExHelpers.h"
#include "Engine/StreamableManager.h"

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExContext : public FPCGContext
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
	
	void StartAsyncWork();
	void StopAsyncWork();
	
	virtual void OnComplete();

#pragma region Async resource management

	void CancelAssetLoading();
	bool WasAssetLoadRequested() const { return bAssetLoadRequested; }
	bool HasAssetRequirements() const { return !RequiredAssets.IsEmpty(); }

	virtual void RegisterAssetDependencies();
	void RegisterAssetRequirement(const FSoftObjectPath& Dependency);
	void LoadAssets();

protected
:
	bool bForceSynchronousAssetLoad = false;
	bool bAssetLoadRequested = false;
	bool bAssetLoadError = false;
	TSet<FSoftObjectPath> RequiredAssets;

	/** Handle holder for any loaded resources */
	TSharedPtr<FStreamableHandle> LoadHandle;

#pragma endregion
};
