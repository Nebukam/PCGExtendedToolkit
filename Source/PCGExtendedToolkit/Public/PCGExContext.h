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
	
	TArray<FPCGTaggedData> StagedOutputs;
	bool bFlattenOutput = false;
	bool bUseLock = true;

	int32 LastReserve = 0;
	int32 AdditionsSinceLastReserve = 0;

	TSet<UObject*> ManagedObjects;

	void CommitStagedOutputs();

public:
	mutable FRWLock ManagedObjectLock; //ugh
	
	virtual ~FPCGExContext() override;

	void StagedOutputReserve(const int32 NumAdditions);
	
	void StageOutput(const FName Pin, UPCGData* InData, const TSet<FString>& InTags, bool bManaged);
	void StageOutput(const FName Pin, UPCGData* InData, bool bManaged);
	
	void AddManagedObject(UObject* InObject);
	void ReleaseManagedObject(UObject* InObject);

	void StartAsyncWork();
	void StopAsyncWork();

	template <class T, typename... Args>
	T* NewManagedObject(Args&&... InArgs)
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

			AddManagedObject(Object);
			
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		}
		else
		{
			FWriteScopeLock WriteScopeLock(ManagedObjectLock);
			Object = FPCGContext::NewObject_AnyThread<T>(this, std::forward<Args>(InArgs)...);
		}
#endif

		return Object;
	}

	void DeleteManagedObject(UObject* InObject);
	
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
