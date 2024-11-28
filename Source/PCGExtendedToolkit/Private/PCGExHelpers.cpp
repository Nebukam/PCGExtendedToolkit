// Copyright Timothé Lapetite 2024
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
		// Flush remaining managed objects & mark them as garbage
		for (UObject* ObjectPtr : ManagedObjects)
		{
			//if (!IsValid(ObjectPtr)) { continue; }
			ObjectPtr->RemoveFromRoot();
			RecursivelyClearAsyncFlag(ObjectPtr);

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
		if (!IsValid(InObject)) { return; }

		{
			FWriteScopeLock WriteScopeLock(ManagedObjectLock);
			ManagedObjects.Add(InObject);
		}

		InObject->AddToRoot();
	}

	bool FManagedObjects::Remove(UObject* InObject)
	{
		if (!IsValid(InObject) || !ManagedObjects.Contains(InObject)) { return false; }

		{
			FWriteScopeLock WriteScopeLock(ManagedObjectLock);
			ManagedObjects.Remove(InObject);
		}

		InObject->RemoveFromRoot();
		RecursivelyClearAsyncFlag(InObject);

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

		if (!IsValid(InObject) || !ManagedObjects.Contains(InObject)) { return; }

		Remove(InObject);
		InObject->MarkAsGarbage();
	}

	void FManagedObjects::RecursivelyClearAsyncFlag(UObject* InObject) const
	{
#if PCGEX_ENGINE_VERSION >= 505
		if (DuplicateObjects.Contains(InObject))
		{
			//UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(InObject);
			//if(SpatialData && SpatialData->Metadata){SpatialData->Metadata->ClearInternalFlags(EInternalObjectFlags::Async);}
			return;
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
