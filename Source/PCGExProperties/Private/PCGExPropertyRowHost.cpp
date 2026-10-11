// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPropertyRowHost.h"

#if WITH_EDITOR

#include "PCGExProperty.h"
#include "PCGExPropertySchemaAsset.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace PCGExPropertyRowHost
{
	void GetLoadedImporters(const UPCGExPropertySchemaAsset* Asset, TArray<TWeakObjectPtr<UObject>>& OutHosts, const bool bIncludeTemplates)
	{
		if (!Asset)
		{
			return;
		}

		// The interface is implemented natively, so its roots are the classes implementing it whose parent does not.
		// Walking those keeps the search to host objects: iterating every UObject holds the object array's lock.
		const UClass* Interface = UPCGExPropertyRowHost::StaticClass();
		TArray<const UClass*, TInlineAllocator<8>> RootClasses;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			const UClass* Parent = It->GetSuperClass();
			if (It->ImplementsInterface(Interface) && !(Parent && Parent->ImplementsInterface(Interface)))
			{
				RootClasses.Add(*It);
			}
		}

		TArray<UObject*> Candidates;
		for (const UClass* RootClass : RootClasses)
		{
			Candidates.Reset();
			GetObjectsOfClass(RootClass, Candidates, /*bIncludeDerivedClasses=*/true);
			for (UObject* Candidate : Candidates)
			{
				if (!IsValid(Candidate) || Candidate->HasAnyFlags(RF_Transient) || (!bIncludeTemplates && Candidate->IsTemplate()))
				{
					continue;
				}
				const UPackage* Package = Candidate->GetPackage();
				if (!Package || Package == GetTransientPackage() || Package->HasAnyPackageFlags(PKG_PlayInEditor))
				{
					continue;
				}

				const IPCGExPropertyRowHost* RowHost = Cast<IPCGExPropertyRowHost>(Candidate);
				const FPCGExPropertySchemaCollection* Schema = RowHost ? RowHost->EDITOR_GetRowSchema() : nullptr;
				if (Schema && Schema->ImportsAssetTransitive(Asset))
				{
					OutHosts.Add(Candidate);
				}
			}
		}
	}

	void BroadcastRowsRebuilt(UObject* Host, const FName RowsMember)
	{
		if (FProperty* RowsProperty = FindFProperty<FProperty>(Host->GetClass(), RowsMember))
		{
			FPropertyChangedEvent RefreshEvent(RowsProperty, EPropertyChangeType::ArrayClear);
			FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(Host, RefreshEvent);
		}
	}

	FEditKind ClassifyEdit(const FPropertyChangedEvent& Event, const FName SchemaMember, const FName RowsMember)
	{
		FEditKind Kind;
		if (!Event.MemberProperty || Event.ChangeType == EPropertyChangeType::Interactive)
		{
			return Kind;
		}

		const FName MemberName = Event.MemberProperty->GetFName();
		// The owner test catches an edit reported against a schema row itself rather than the member holding it.
		if (MemberName == SchemaMember || Event.MemberProperty->GetOwnerStruct() == FPCGExPropertySchema::StaticStruct())
		{
			Kind.bSyncRows = true;
			Kind.bSchemaChanged = true;
		}
		else if (MemberName == RowsMember)
		{
			Kind.bSyncRows = Event.ChangeType == EPropertyChangeType::ArrayAdd
				|| Event.ChangeType == EPropertyChangeType::ArrayRemove
				|| Event.ChangeType == EPropertyChangeType::ArrayClear
				|| Event.ChangeType == EPropertyChangeType::ArrayMove;
		}
		return Kind;
	}
}

#endif
