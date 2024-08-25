// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Runtime/Launch/Resources/Version.h"

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "Engine/StreamableManager.h"

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExContext : public FPCGContext
{
protected:
	mutable FRWLock ContextOutputLock;
	TArray<FPCGTaggedData> FutureOutputs;
	TArray<UPCGData*> Rooted;
	bool bFlattenOutput = false;
	bool bUseLock = true;

	int32 LastReserve = 0;
	int32 AdditionsSinceLastReserve = 0;

	void WriteFutureOutputs();

public:
	virtual ~FPCGExContext() override;

	void UnrootFutures();

	void FutureReserve(const int32 NumAdditions);
	void FutureRootedOutput(const FName Pin, UPCGData* InData, const TSet<FString>& InTags);
	void FutureOutput(const FName Pin, UPCGData* InData, const TSet<FString>& InTags);
	void FutureOutput(const FName Pin, UPCGData* InData);

	
	virtual void OnComplete();
	
#pragma region Async resource management

	// !! Note !! : This class retrofits async resource loading from later PCG context code in order to be retro-compatible with 5.3
	
	/** Request a load. If load was already requested, do nothing. LoadHandle will be set in the context, meaning that assets will stay alive while context is loaded.
	* Request can be synchronous or asynchronous. If loading is asynchronous, the current task is paused and will be woken up when the loading is done.
	* WARNING: Make sure to call this function with soft paths that are NOT null.
	* Returns true if the execution can continue (objects are loaded or invalid), or false if we need to wait for loading
	*/
	bool RequestResourceLoad(FPCGContext* ThisContext, TArray<FSoftObjectPath>&& ObjectsToLoad, bool bAsynchronous = true);
	void CancelLoading();
	bool WasLoadRequested() const { return bLoadRequested; }

private:
	
	/** If the load was already requested */
	bool bLoadRequested = false;

	/** Handle holder for any loaded resources */
	TSharedPtr<FStreamableHandle> LoadHandle;

#pragma endregion 
};
