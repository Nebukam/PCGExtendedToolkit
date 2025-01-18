// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExHelpers.h"

namespace PCGEx
{
	void FIntTracker::IncrementPending(const int32 Count)
	{
		{
			FReadScopeLock ReadScopeLock(Lock);
			if (bTriggered) { return; }
		}
		{
			FWriteScopeLock WriteScopeLock(Lock);
			if (PendingCount == 0 && StartFn) { StartFn(); }
			PendingCount += Count;
		}
	}

	void FIntTracker::IncrementCompleted(const int32 Count)
	{
		{
			FReadScopeLock ReadScopeLock(Lock);
			if (bTriggered) { return; }
		}
		{
			FWriteScopeLock WriteScopeLock(Lock);
			CompletedCount += Count;
			if (CompletedCount == PendingCount) { TriggerInternal(); }
		}
	}

	void FIntTracker::Trigger()
	{
		FWriteScopeLock WriteScopeLock(Lock);
		TriggerInternal();
	}

	void FIntTracker::SafetyTrigger()
	{
		FWriteScopeLock WriteScopeLock(Lock);
		if (PendingCount > 0) { TriggerInternal(); }
	}

	void FIntTracker::Reset()
	{
		FWriteScopeLock WriteScopeLock(Lock);
		PendingCount = CompletedCount = 0;
		bTriggered = false;
	}

	void FIntTracker::Reset(const int32 InMax)
	{
		FWriteScopeLock WriteScopeLock(Lock);
		PendingCount = InMax;
		CompletedCount = 0;
		bTriggered = false;
	}

	void FIntTracker::TriggerInternal()
	{
		if (bTriggered) { return; }
		bTriggered = true;
		ThresholdFn();
		PendingCount = CompletedCount = 0;
	}

	FName FUniqueNameGenerator::Get(const FString& BaseName)
	{
		FName OutName = FName(BaseName + "_" + FString::Printf(TEXT("%d"), Idx));
		FPlatformAtomics::InterlockedIncrement(&Idx);
		return OutName;
	}

	FName FUniqueNameGenerator::Get(const FName& BaseName)
	{
		return Get(BaseName.ToString());
	}

	FManagedObjects::~FManagedObjects()
	{
		Flush();
	}

	bool FManagedObjects::IsAvailable() const
	{
		FReadScopeLock WriteScopeLock(ManagedObjectLock);
		return WorkPermit.IsValid() && !IsFlushing();
	}

	void FManagedObjects::Flush()
	{
		bool bExpected = false;
		if (bIsFlushing.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
		{
			FWriteScopeLock WriteScopeLock(ManagedObjectLock);

			// Flush remaining managed objects & mark them as garbage
			for (UObject* ObjectPtr : ManagedObjects)
			{
				//if (!IsValid(ObjectPtr)) { continue; }
				ObjectPtr->RemoveFromRoot();
				RecursivelyClearAsyncFlag_Unsafe(ObjectPtr);

				if (ObjectPtr->Implements<UPCGExManagedObjectInterface>())
				{
					if (IPCGExManagedObjectInterface* ManagedObject = Cast<IPCGExManagedObjectInterface>(ObjectPtr)) { ManagedObject->Cleanup(); }
				}

				ObjectPtr->MarkAsGarbage();
			}

			ManagedObjects.Empty();
			bIsFlushing.store(false, std::memory_order_release);
		}
	}

	void FManagedObjects::Add(UObject* InObject)
	{
		check(!IsFlushing())

		if (!IsValid(InObject)) { return; }

		{
			FWriteScopeLock WriteScopeLock(ManagedObjectLock);
			ManagedObjects.Add(InObject);
			InObject->AddToRoot();
		}
	}

	bool FManagedObjects::Remove(UObject* InObject)
	{
		if (IsFlushing()) { return false; } // Will be removed anyway

		{
			FWriteScopeLock WriteScopeLock(ManagedObjectLock);

			if (!IsValid(InObject) || !ManagedObjects.Contains(InObject)) { return false; }

			ManagedObjects.Remove(InObject);

			InObject->RemoveFromRoot();
			RecursivelyClearAsyncFlag_Unsafe(InObject);
		}

		if (InObject->Implements<UPCGExManagedObjectInterface>())
		{
			IPCGExManagedObjectInterface* ManagedObject = Cast<IPCGExManagedObjectInterface>(InObject);
			if (ManagedObject) { ManagedObject->Cleanup(); }
		}

		return true;
	}

	void FManagedObjects::Destroy(UObject* InObject)
	{
		check(InObject)

		if (IsFlushing()) { return; } // Will be destroyed anyway

		{
			FReadScopeLock ReadScopeLock(ManagedObjectLock);
			if (!IsValid(InObject) || !ManagedObjects.Contains(InObject)) { return; }
		}

		Remove(InObject);
		InObject->MarkAsGarbage();
	}

	void FManagedObjects::RecursivelyClearAsyncFlag_Unsafe(UObject* InObject) const
	{
#if PCGEX_ENGINE_VERSION >= 505
		{
			FReadScopeLock ReadScopeLock(DuplicatedObjectLock);
			if (DuplicateObjects.Contains(InObject)) { return; }
		}
#endif

		if (InObject->HasAnyInternalFlags(EInternalObjectFlags::Async))
		{
			InObject->ClearInternalFlags(EInternalObjectFlags::Async);

			ForEachObjectWithOuter(
				InObject, [&](UObject* SubObject)
				{
					if (ManagedObjects.Contains(SubObject)) { return; }
					SubObject->ClearInternalFlags(EInternalObjectFlags::Async);
				}, true);
		}
	}
}

void UPCGExComponentCallback::Callback(UActorComponent* InComponent)
{
	if (!CallbackFn) { return; }
	if (bIsOnce)
	{
		auto Callback = MoveTemp(CallbackFn);
		CallbackFn = nullptr;
		Callback(InComponent);
	}
	else
	{
		CallbackFn(InComponent);
	}
}

void UPCGExPCGComponentCallback::Callback(UPCGComponent* InComponent)
{
	if (!CallbackFn) { return; }
	if (bIsOnce)
	{
		auto Callback = MoveTemp(CallbackFn);
		CallbackFn = nullptr;
		Callback(InComponent);
	}
	else
	{
		CallbackFn(InComponent);
	}
}
