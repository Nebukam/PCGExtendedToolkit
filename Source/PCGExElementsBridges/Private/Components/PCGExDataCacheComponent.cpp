// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Components/PCGExDataCacheComponent.h"

#include "GameFramework/Actor.h"
#include "Misc/ScopeRWLock.h"
#include "UObject/Package.h"

#include "PCGExLog.h"

#define LOCTEXT_NAMESPACE "PCGExDataCacheComponent"

namespace PCGExDataCacheComponent
{
	// Caller holds the read lock.
	void ViewEntries(const TMap<FName, FPCGExDataCacheEntry>& InMap, TMap<FName, const FPCGDataCollection*>& OutView)
	{
		for (const TPair<FName, FPCGExDataCacheEntry>& Pair : InMap)
		{
			OutView.Add(Pair.Key, &Pair.Value.Data);
		}
	}
}

#pragma region UPCGExDataCacheComponent

UPCGExDataCacheComponent::UPCGExDataCacheComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetFlags(RF_Transactional);
}

UPCGExDataCacheComponent* UPCGExDataCacheComponent::Find(const AActor* InActor)
{
	return IsValid(InActor) ? InActor->FindComponentByClass<UPCGExDataCacheComponent>() : nullptr;
}

UPCGExDataCacheComponent* UPCGExDataCacheComponent::FindOrCreate(AActor* InActor)
{
	check(IsInGameThread());

	if (!IsValid(InActor)) { return nullptr; }
	if (UPCGExDataCacheComponent* Existing = Find(InActor)) { return Existing; }

	// Instance components survive construction-script reruns and are not touched by PCG cleanup; same path as
	// the engine's Add Component node.
	UPCGExDataCacheComponent* Component = NewObject<UPCGExDataCacheComponent>(InActor, NAME_None, RF_Transactional);
	Component->RegisterComponent();
	InActor->AddInstanceComponent(Component);
	return Component;
}

bool UPCGExDataCacheComponent::Read(const FName InId, FPCGDataCollection& OutData) const
{
	UE::TReadScopeLock ScopedReadLock(Lock);

	if (const FPCGExDataCacheEntry* Preview = PreviewEntries.Find(InId))
	{
		OutData = Preview->Data;
		return true;
	}

	if (const FPCGExDataCacheEntry* Entry = Entries.Find(InId))
	{
		OutData = Entry->Data;
		return true;
	}

	return false;
}

void UPCGExDataCacheComponent::ReadAll(TArray<TPair<FName, FPCGDataCollection>>& OutEntries) const
{
	UE::TReadScopeLock ScopedReadLock(Lock);

	TMap<FName, const FPCGDataCollection*> View;
	PCGExDataCacheComponent::ViewEntries(Entries, View);
	PCGExDataCacheComponent::ViewEntries(PreviewEntries, View); // Preview shadows persisted on key collision.

	OutEntries.Reserve(OutEntries.Num() + View.Num());
	for (const TPair<FName, const FPCGDataCollection*>& Pair : View)
	{
		OutEntries.Emplace(Pair.Key, *Pair.Value);
	}
}

bool UPCGExDataCacheComponent::Contains(const FName InId) const
{
	UE::TReadScopeLock ScopedReadLock(Lock);
	return PreviewEntries.Contains(InId) || Entries.Contains(InId);
}

int32 UPCGExDataCacheComponent::NumEntries() const
{
	UE::TReadScopeLock ScopedReadLock(Lock);

	TSet<FName> Keys;
	Entries.GetKeys(Keys);
	for (const TPair<FName, FPCGExDataCacheEntry>& Pair : PreviewEntries) { Keys.Add(Pair.Key); }
	return Keys.Num();
}

void UPCGExDataCacheComponent::Write(const FName InId, const EPCGExDataCacheWriteMode InMode, TArray<FPCGTaggedData>&& InData, const UObject* InWriter, const bool bPreview)
{
	check(IsInGameThread());

	if (InMode == EPCGExDataCacheWriteMode::ClearAll)
	{
		ClearAll();
		return;
	}

	if (InMode == EPCGExDataCacheWriteMode::Remove)
	{
		Remove(InId);
		return;
	}

	// Rename/Flatten happen outside the lock; the lock only brackets the map swap.
	TArray<FPCGTaggedData> Adopted = AdoptData(MoveTemp(InData), bPreview);

	FPCGDataCollection Released;
	{
		UE::TWriteScopeLock ScopedWriteLock(Lock);

		TMap<FName, FPCGExDataCacheEntry>& Map = bPreview ? PreviewEntries : Entries;
		FPCGExDataCacheEntry& Entry = Map.FindOrAdd(InId);

		if (InMode == EPCGExDataCacheWriteMode::Replace)
		{
			Released = MoveTemp(Entry.Data);
			Entry.Data.Reset();
		}

		Entry.Data.TaggedData.Append(MoveTemp(Adopted));
		Entry.Writer = FSoftObjectPath(InWriter);
		Entry.Revision++;
	}

	ReleaseData(Released);

	if (!bPreview) { MarkPackageDirty(); }
}

void UPCGExDataCacheComponent::Remove(const FName InId)
{
	check(IsInGameThread());

	FPCGDataCollection ReleasedPersisted;
	FPCGDataCollection ReleasedPreview;
	bool bDirty = false;
	{
		UE::TWriteScopeLock ScopedWriteLock(Lock);

		if (FPCGExDataCacheEntry* Entry = Entries.Find(InId))
		{
			ReleasedPersisted = MoveTemp(Entry->Data);
			Entries.Remove(InId);
			bDirty = true;
		}

		if (FPCGExDataCacheEntry* Entry = PreviewEntries.Find(InId))
		{
			ReleasedPreview = MoveTemp(Entry->Data);
			PreviewEntries.Remove(InId);
		}
	}

	ReleaseData(ReleasedPersisted);
	ReleaseData(ReleasedPreview);

	if (bDirty) { MarkPackageDirty(); }
}

void UPCGExDataCacheComponent::ClearAll()
{
	check(IsInGameThread());

	TArray<FPCGDataCollection> Released;
	bool bDirty = false;
	{
		UE::TWriteScopeLock ScopedWriteLock(Lock);

		Released.Reserve(Entries.Num() + PreviewEntries.Num());
		for (TPair<FName, FPCGExDataCacheEntry>& Pair : Entries) { Released.Add(MoveTemp(Pair.Value.Data)); }
		for (TPair<FName, FPCGExDataCacheEntry>& Pair : PreviewEntries) { Released.Add(MoveTemp(Pair.Value.Data)); }

		bDirty = !Entries.IsEmpty();
		Entries.Reset();
		PreviewEntries.Reset();
	}

	for (const FPCGDataCollection& Collection : Released) { ReleaseData(Collection); }

	if (bDirty) { MarkPackageDirty(); }
}

#if WITH_EDITOR
void UPCGExDataCacheComponent::EDITOR_ClearCache()
{
	Modify();
	ClearAll();
}
#endif

void UPCGExDataCacheComponent::ReleaseData(const FPCGDataCollection& InData) const
{
	// Mirrors UPCGComponent::ClearGraphGeneratedOutput: only objects we own go back to the transient package. Any
	// context still holding the data keeps it alive; GC reclaims it once the last reference drops.
	for (const FPCGTaggedData& TaggedData : InData.TaggedData)
	{
		if (!TaggedData.Data) { continue; }

		TaggedData.Data->VisitDataNetwork([this](const UPCGData* InNetworkData)
		{
			if (InNetworkData && InNetworkData->GetOuter() == this)
			{
				const_cast<UPCGData*>(InNetworkData)->Rename(nullptr, GetTransientPackage(), REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
			}
		});
	}
}

TArray<FPCGTaggedData> UPCGExDataCacheComponent::AdoptData(TArray<FPCGTaggedData>&& InData, const bool bPreview) const
{
	TArray<FPCGTaggedData> Adopted;
	Adopted.Reserve(InData.Num());

	const ERenameFlags RenameFlags = bPreview ? REN_DoNotDirty : REN_None;
	UPCGExDataCacheComponent* NewOuter = const_cast<UPCGExDataCacheComponent*>(this);

	for (FPCGTaggedData& TaggedData : InData)
	{
		if (!TaggedData.Data) { continue; }

		// Proxies (render targets, ...) hold non-serializable resources; the engine refuses them on component output too.
		if (!TaggedData.Data->CanBeSerialized())
		{
			UE_LOG(LogPCGEx, Warning, TEXT("[Data Cache] '%s' cannot be serialized and was not cached."), *TaggedData.Data->GetName());
			continue;
		}

		// Same order as UPCGComponent::PostProcessGraph: flatten first, then re-outer the whole network.
		TaggedData.Data->VisitDataNetwork([](const UPCGData* InNetworkData)
		{
			if (InNetworkData) { const_cast<UPCGData*>(InNetworkData)->Flatten(); }
		});

		TaggedData.Data->VisitDataNetwork([NewOuter, bPreview, RenameFlags](const UPCGData* InNetworkData)
		{
			if (!InNetworkData) { return; }
			UPCGData* Mutable = const_cast<UPCGData*>(InNetworkData);
			if (bPreview) { Mutable->SetFlags(RF_Transient); }
			else { Mutable->ClearFlags(RF_Transient); }
			Mutable->Rename(nullptr, NewOuter, RenameFlags);
		});

		Adopted.Add(MoveTemp(TaggedData));
	}

	return Adopted;
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
