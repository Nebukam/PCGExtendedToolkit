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
				IPCGExManagedObjectInterface* ManagedObject = Cast<IPCGExManagedObjectInterface>(ObjectPtr);
				if (ManagedObject) { ManagedObject->Cleanup(); }
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

	void FManagedObjects::Remove(UObject* InObject)
	{
		if (!IsValid(InObject) || !ManagedObjects.Contains(InObject)) { return; }

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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
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
