// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetSelectors/PCGExActorCollection.h"

#include "PCGEx.h"
#include "PCGExMacros.h"

bool FPCGExActorCollectionEntry::Validate(const UPCGExAssetCollection* ParentCollection)
{
	if (bIsSubCollection) { LoadSubCollection(SubCollection); }
	else if (!Actor && ParentCollection->bDoNotIgnoreInvalidEntries) { return false; }

	return Super::Validate(ParentCollection);
}

#if WITH_EDITOR
void FPCGExActorCollectionEntry::UpdateStaging(const bool bRecursive)
{
	if (bIsSubCollection)
	{
		if (bRecursive && SubCollection.LoadSynchronous()) { SubCollection.Get()->RefreshStagingData_Recursive(); }
		return;
	}

	Staging.Path = Actor.ToSoftObjectPath();
	const AActor* A = Actor.LoadSynchronous();

	if (!A)
	{
		Staging.Pivot = FVector::ZeroVector;
		Staging.Bounds = FBox(ForceInitToZero);
		return;
	}

	FVector Origin = FVector::ZeroVector;
	FVector Extents = FVector::ZeroVector;
	A->GetActorBounds(true, Origin, Extents);

	Staging.Pivot = Origin;
	Staging.Bounds = FBoxCenterAndExtent(Origin, Extents).GetBox();
	
	Super::UpdateStaging(bRecursive);
}
#endif

void FPCGExActorCollectionEntry::OnSubCollectionLoaded()
{
	SubCollectionPtr = Cast<UPCGExActorCollection>(BaseSubCollectionPtr);
}

#if WITH_EDITOR
bool UPCGExActorCollection::IsCacheableProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Super::IsCacheableProperty(PropertyChangedEvent)) { return true; }
	return PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UPCGExActorCollection, Entries);
}


void UPCGExActorCollection::RefreshDisplayNames()
{
	Super::RefreshDisplayNames();
	for (FPCGExActorCollectionEntry& Entry : Entries)
	{
		Entry.DisplayName = Entry.bIsSubCollection ? FName(TEXT("[") + Entry.SubCollection.GetAssetName() + TEXT("]")) : FName(Entry.Actor.GetAssetName());
	}
}

void UPCGExActorCollection::RefreshStagingData()
{
	for (FPCGExActorCollectionEntry& Entry : Entries) { Entry.UpdateStaging(false); }
	Super::RefreshStagingData();
}

void UPCGExActorCollection::RefreshStagingData_Recursive()
{
	for (FPCGExActorCollectionEntry& Entry : Entries) { Entry.UpdateStaging(true); }
	Super::RefreshStagingData_Recursive();
}
#endif

void UPCGExActorCollection::BuildCache()
{
	Super::BuildCache(Entries);
}
