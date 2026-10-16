// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPropertyRowHost.h"

#if WITH_EDITOR

#include "PCGExProperty.h"
#include "PCGExPropertySchema.h"
#include "PCGExPropertySchemaAsset.h"
#include "UObject/Package.h"
#include "UObject/StructOnScope.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

bool IPCGExPropertyRowHost::EDITOR_ResolveInheritedValue(const FPCGExPropertyRowSite& Site, const FName PropertyName, FInstancedStruct& Out) const
{
	const FPCGExPropertySchemaCollection* Schema = EDITOR_GetRowSchema();
	if (!Schema || PropertyName.IsNone())
	{
		return false;
	}

	FLayerChain Chain;
	EDITOR_GetRowSchemaFallbackChain(Chain);
	return PCGExPropertyRowHost::ResolveDeclaredValue(*Schema, MakeArrayView(Chain), /*bIncludeOwnOverrides=*/!Site.bIsImportLayer, PropertyName, Out);
}

namespace PCGExPropertyRowHost
{
	bool ResolveDeclaredValue(
		const FPCGExPropertySchemaCollection& Schema, const TConstArrayView<const FPCGExPropertyOverrides*> FallbackChain, const bool bIncludeOwnOverrides,
		const FName PropertyName, FInstancedStruct& Out)
	{
		if (PropertyName.IsNone())
		{
			return false;
		}

		TArray<FPCGExPropertyResolved> Resolved;
		Schema.Resolve(Resolved, FallbackChain, bIncludeOwnOverrides);
		const FPCGExPropertyResolved* Match = Resolved.FindByPredicate([PropertyName](const FPCGExPropertyResolved& Candidate)
		{
			return Candidate.Source->Name == PropertyName;
		});
		if (!Match)
		{
			return false;
		}

		FInstancedStruct Copy = Match->MakeEffectiveCopy();
		if (!Copy.IsValid())
		{
			return false;
		}
		Out = MoveTemp(Copy);
		return true;
	}

	struct FScopeResolverEntry
	{
		TWeakPtr<FStructOnScope> Scope;
		FResolveInheritedValue Resolver;
		uint64 Serial = 0;
	};

	// Keyed by scope address; a registration's Serial tells its own entry from a later one at a reused address.
	TMap<const FStructOnScope*, FScopeResolverEntry>& ScopeResolverEntries()
	{
		static TMap<const FStructOnScope*, FScopeResolverEntry> Entries;
		return Entries;
	}

	FScopeResolverRegistration::FScopeResolverRegistration(const TSharedRef<FStructOnScope>& InScope, FResolveInheritedValue InResolver)
		: Key(&InScope.Get())
	{
		static uint64 NextSerial = 1;
		Serial = NextSerial++;

		FScopeResolverEntry& Entry = ScopeResolverEntries().FindOrAdd(Key);
		Entry.Scope = InScope;
		Entry.Resolver = MoveTemp(InResolver);
		Entry.Serial = Serial;
	}

	FScopeResolverRegistration::~FScopeResolverRegistration()
	{
		TMap<const FStructOnScope*, FScopeResolverEntry>& Entries = ScopeResolverEntries();
		const FScopeResolverEntry* Entry = Entries.Find(Key);
		if (Entry && Entry->Serial == Serial)
		{
			Entries.Remove(Key);
		}
	}

	const FResolveInheritedValue* FindScopeResolver(const FStructOnScope* Scope)
	{
		TMap<const FStructOnScope*, FScopeResolverEntry>& Entries = ScopeResolverEntries();
		const FScopeResolverEntry* Entry = Scope ? Entries.Find(Scope) : nullptr;
		if (!Entry)
		{
			return nullptr;
		}
		if (!Entry->Scope.IsValid())
		{
			// The panel dropped the scope before its registration: nothing to answer for.
			Entries.Remove(Scope);
			return nullptr;
		}
		return &Entry->Resolver;
	}

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
