// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExHelpers.h"

namespace PCGEx
{
	FManagedObjects::~FManagedObjects()
	{
		Flush();
	}

	void FManagedObjects::Flush()
	{
		FWriteScopeLock WriteScopeLock(ManagedObjectLock);
		FPlatformAtomics::InterlockedExchange(&bFlushing, 1);

		// Flush remaining managed objects & mark them as garbage
		for (UObject* ObjectPtr : ManagedObjects)
		{
			//if (!IsValid(ObjectPtr)) { continue; }
			ObjectPtr->RemoveFromRoot();
			RecursivelyClearAsyncFlagUnsafe(ObjectPtr);

			if (ObjectPtr->Implements<UPCGExManagedObjectInterface>())
			{
				if (IPCGExManagedObjectInterface* ManagedObject = Cast<IPCGExManagedObjectInterface>(ObjectPtr)) { ManagedObject->Cleanup(); }
			}

			ObjectPtr->MarkAsGarbage();
		}

		ManagedObjects.Empty();
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
			RecursivelyClearAsyncFlagUnsafe(InObject);
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

	void FManagedObjects::RecursivelyClearAsyncFlagUnsafe(UObject* InObject) const
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
