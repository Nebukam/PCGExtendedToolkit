// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetSelectors/PCGExActorCollection.h"

#include "PCGEx.h"
#include "PCGExMacros.h"

bool FPCGExActorCollectionEntry::IsValid()
{
	if (bIsSubCollection)
	{
		LoadSubCollection(SubCollection);
		if (!SubCollectionPtr || SubCollectionPtr->GetValidEntryNum() == 0) { return false; }
	}
	else if (!Actor) { return false; }

	return true;
}

void FPCGExActorCollectionEntry::OnSubCollectionLoaded()
{
	SubCollectionPtr = Cast<UPCGExActorCollection>(BaseSubCollectionPtr);
	BaseSubCollectionPtr->RebuildCachedData(SubCollectionPtr->Entries);
}


#if WITH_EDITOR
void UPCGExActorCollection::RefreshDisplayNames()
{
	Super::RefreshDisplayNames();
	for (FPCGExActorCollectionEntry& Entry : Entries)
	{
		Entry.DisplayName = Entry.bIsSubCollection ? FName(TEXT("[") + Entry.SubCollection.GetAssetName() + TEXT("]")) : FName(Entry.Actor.GetAssetName());
	}
}
#endif
