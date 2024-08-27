// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetSelectors/PCGExMeshCollection.h"

#include "PCGEx.h"
#include "PCGExMacros.h"

bool FPCGExMeshCollectionEntry::IsValid()
{
	if (bIsSubCollection)
	{
		LoadSubCollection(SubCollection);
		if (!SubCollectionPtr || SubCollectionPtr->GetValidEntryNum() == 0) { return false; }
	}
	else if (!Descriptor.StaticMesh.ToSoftObjectPath().IsValid()) { return false; }

	return true;
}

void FPCGExMeshCollectionEntry::OnSubCollectionLoaded()
{
	SubCollectionPtr = Cast<UPCGExMeshCollection>(BaseSubCollectionPtr);
	BaseSubCollectionPtr->RebuildCachedData(SubCollectionPtr->Entries);
}


#if WITH_EDITOR
void UPCGExMeshCollection::RefreshDisplayNames()
{
	Super::RefreshDisplayNames();
	for (FPCGExMeshCollectionEntry& Entry : Entries)
	{
		Entry.DisplayName = Entry.bIsSubCollection ? FName(TEXT("[") + Entry.SubCollection.GetAssetName() + TEXT("]")) : FName(Entry.Descriptor.StaticMesh.GetAssetName());
	}
}

#endif
