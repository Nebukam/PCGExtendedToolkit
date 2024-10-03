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
			if (!IsValid(ObjectPtr)) { continue; }
			if (ObjectPtr->IsRooted()) { ObjectPtr->RemoveFromRoot(); }
			if (ObjectPtr->HasAnyInternalFlags(EInternalObjectFlags::Async))
			{
				ObjectPtr->ClearInternalFlags(EInternalObjectFlags::Async);

				ForEachObjectWithOuter(
					ObjectPtr, [&](UObject* SubObject)
					{
						if (ManagedObjects.Contains(SubObject)) { return; }
						SubObject->ClearInternalFlags(EInternalObjectFlags::Async);
					}, true);
			}

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

		FWriteScopeLock WriteScopeLock(ManagedObjectLock);
		ManagedObjects.Add(InObject);
		InObject->AddToRoot();
	}

	void FManagedObjects::Remove(UObject* InObject)
	{
		if (!IsValid(InObject)) { return; }

		{
			FWriteScopeLock WriteScopeLock(ManagedObjectLock);
			ManagedObjects.Remove(InObject);
			if (InObject->IsRooted()) { InObject->RemoveFromRoot(); }

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

		if (InObject->Implements<UPCGExManagedObjectInterface>())
		{
			IPCGExManagedObjectInterface* ManagedObject = Cast<IPCGExManagedObjectInterface>(InObject);
			if (ManagedObject) { ManagedObject->Cleanup(); }
		}
	}

	void FManagedObjects::Destroy(UObject* InObject)
	{
		check(InObject)

		if (!IsValid(InObject)) { return; }

		Remove(InObject);
		InObject->MarkAsGarbage();
	}
}
