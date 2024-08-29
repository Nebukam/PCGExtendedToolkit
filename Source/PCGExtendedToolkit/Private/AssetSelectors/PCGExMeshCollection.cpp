// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetSelectors/PCGExMeshCollection.h"

#include "PCGEx.h"
#include "PCGExMacros.h"

bool FPCGExMeshCollectionEntry::Validate(const UPCGExAssetCollection* ParentCollection)
{
	if (bIsSubCollection) { LoadSubCollection(SubCollection); }
	else if (!Descriptor.StaticMesh.ToSoftObjectPath().IsValid() && ParentCollection->bDoNotIgnoreInvalidEntries) { return false; }

	return Super::Validate(ParentCollection);
}

#if WITH_EDITOR
void FPCGExMeshCollectionEntry::UpdateStaging(const UPCGExAssetCollection* OwningCollection, const bool bRecursive)
{
	if (bIsSubCollection)
	{
		if (bRecursive && SubCollection.LoadSynchronous()) { SubCollection.Get()->RefreshStagingData_Recursive(); }
		return;
	}

	Staging.Path = Descriptor.StaticMesh.ToSoftObjectPath();
	
	const UStaticMesh* M = Descriptor.StaticMesh.LoadSynchronous();
	if (!M)
	{
		Staging.Pivot = FVector::ZeroVector;
		Staging.Bounds = FBox(ForceInitToZero);
		return;
	}

	Staging.Pivot = FVector::ZeroVector;
	Staging.Bounds = M->GetBoundingBox();

	Super::UpdateStaging(OwningCollection, bRecursive);
	
}
#endif

void FPCGExMeshCollectionEntry::OnSubCollectionLoaded()
{
	SubCollectionPtr = Cast<UPCGExMeshCollection>(BaseSubCollectionPtr);
}

#if WITH_EDITOR
bool UPCGExMeshCollection::IsCacheableProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Super::IsCacheableProperty(PropertyChangedEvent)) { return true; }
	return PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UPCGExMeshCollection, Entries);
}

void UPCGExMeshCollection::RefreshDisplayNames()
{
	Super::RefreshDisplayNames();
	for (FPCGExMeshCollectionEntry& Entry : Entries)
	{
		Entry.DisplayName = Entry.bIsSubCollection ? FName(TEXT("[") + Entry.SubCollection.GetAssetName() + TEXT("]")) : FName(Entry.Descriptor.StaticMesh.GetAssetName());
	}
}

void UPCGExMeshCollection::RefreshStagingData()
{
	for (FPCGExMeshCollectionEntry& Entry : Entries) { Entry.UpdateStaging(this, false); }
	Super::RefreshStagingData();
}

void UPCGExMeshCollection::RefreshStagingData_Recursive()
{
	for (FPCGExMeshCollectionEntry& Entry : Entries) { Entry.UpdateStaging(this, true); }
	Super::RefreshStagingData_Recursive();
}

void UPCGExMeshCollection::BuildCache()
{
	Super::BuildCache(Entries);
}
#endif
