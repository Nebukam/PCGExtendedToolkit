// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetSelectors/PCGExActorCollection.h"

bool FPCGExActorCollectionEntry::Validate(const UPCGExAssetCollection* ParentCollection)
{
	if (bIsSubCollection) { LoadSubCollection(SubCollection); }
	else if (!Actor && ParentCollection->bDoNotIgnoreInvalidEntries) { return false; }

	return Super::Validate(ParentCollection);
}

void FPCGExActorCollectionEntry::UpdateStaging(const UPCGExAssetCollection* OwningCollection, const bool bRecursive)
{
	if (bIsSubCollection)
	{
		if (bRecursive && SubCollection.LoadSynchronous()) { SubCollection.Get()->EDITOR_RebuildStagingData_Recursive(); }
		return;
	}

	Staging.Path = Actor.ToSoftObjectPath();
	const AActor* A = Actor.LoadSynchronous();

	PCGExAssetCollection::UpdateStagingBounds(Staging, A);

	Super::UpdateStaging(OwningCollection, bRecursive);
}

void FPCGExActorCollectionEntry::SetAssetPath(FSoftObjectPath InPath)
{
	Actor = TSoftObjectPtr<AActor>(InPath);
}

void FPCGExActorCollectionEntry::OnSubCollectionLoaded()
{
	SubCollectionPtr = Cast<UPCGExActorCollection>(BaseSubCollectionPtr);
}

void UPCGExActorCollection::RebuildStagingData(const bool bRecursive)
{
	for (FPCGExActorCollectionEntry& Entry : Entries) { Entry.UpdateStaging(this, bRecursive); }
	Super::RebuildStagingData(bRecursive);
}

#if WITH_EDITOR
bool UPCGExActorCollection::EDITOR_IsCacheableProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Super::EDITOR_IsCacheableProperty(PropertyChangedEvent)) { return true; }
	return PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UPCGExActorCollection, Entries);
}


void UPCGExActorCollection::EDITOR_RefreshDisplayNames()
{
	Super::EDITOR_RefreshDisplayNames();
	for (FPCGExActorCollectionEntry& Entry : Entries)
	{
		Entry.DisplayName = Entry.bIsSubCollection ? FName(TEXT("[") + Entry.SubCollection.GetAssetName() + TEXT("]")) : FName(Entry.Actor.GetAssetName());
	}
}
#endif

UPCGExAssetCollection* UPCGExActorCollection::GetCollectionFromAttributeSet(const FPCGContext* InContext, const UPCGParamData* InAttributeSet, const FPCGExAssetAttributeSetDetails& Details) const
{
	return GetCollectionFromAttributeSetTpl<UPCGExActorCollection>(InContext, InAttributeSet, Details);
}

UPCGExAssetCollection* UPCGExActorCollection::GetCollectionFromAttributeSet(const FPCGContext* InContext, const FName InputPin, const FPCGExAssetAttributeSetDetails& Details) const
{
	return GetCollectionFromAttributeSetTpl<UPCGExActorCollection>(InContext, InputPin, Details);
}

void UPCGExActorCollection::BuildCache()
{
	Super::BuildCache(Entries);
}
