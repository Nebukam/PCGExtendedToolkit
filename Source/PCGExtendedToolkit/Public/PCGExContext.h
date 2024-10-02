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
	mutable FRWLock ContextOutputLock;
	TArray<FPCGTaggedData> FutureOutputs;
	TArray<UPCGData*> Rooted;
	bool bFlattenOutput = false;
	bool bUseLock = true;

	int32 LastReserve = 0;
	int32 AdditionsSinceLastReserve = 0;

	void WriteFutureOutputs();

public:
	mutable FRWLock AsyncObjectLock; //ugh
	
	virtual ~FPCGExContext() override;

	void UnrootFutures();

	void FutureReserve(const int32 NumAdditions);
	void FutureRootedOutput(const FName Pin, UPCGData* InData, const TSet<FString>& InTags);
	void FutureOutput(const FName Pin, UPCGData* InData, const TSet<FString>& InTags);
	void FutureOutput(const FName Pin, UPCGData* InData);

	void StartAsyncWork();
	void StopAsyncWork();

	template <class T, typename... Args>
	T* PCGExNewObject(Args&&... InArgs)
	{
		PCGEX_ENFORCE_CONTEXT_ASYNC(this)

		T* Object = nullptr;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		if constexpr (!std::is_base_of_v<T, UPCGData>)
		{
			// Since we create ops & factories through this flow, make sure we only use the 5.5 code for UPCGData stuff
#endif

			if (AsyncState.bIsRunningOnMainThread)
			{
				{
					FGCScopeGuard Scope;
					Object = ::NewObject<T>(std::forward<Args>(InArgs)...);
				}
				check(Object);
			}
			else
			{
				Object = ::NewObject<T>(std::forward<Args>(InArgs)...);
			}

			Object->AddToRoot();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		}
		else
		{
			FWriteScopeLock WriteScopeLock(AsyncObjectLock);
			Object = FPCGContext::NewObject_AnyThread<T>(this, std::forward<Args>(InArgs)...);
		}
#endif

		return Object;
	}

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
