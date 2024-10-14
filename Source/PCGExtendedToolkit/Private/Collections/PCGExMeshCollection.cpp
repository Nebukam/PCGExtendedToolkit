// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Collections/PCGExMeshCollection.h"

bool FPCGExMeshCollectionEntry::Validate(const UPCGExAssetCollection* ParentCollection)
{
	if (bIsSubCollection) { LoadSubCollection(SubCollection); }
	else if (!ISMDescriptor.StaticMesh.ToSoftObjectPath().IsValid() && ParentCollection->bDoNotIgnoreInvalidEntries) { return false; }

	return Super::Validate(ParentCollection);
}

void FPCGExMeshCollectionEntry::UpdateStaging(const UPCGExAssetCollection* OwningCollection, const bool bRecursive)
{
	if (bIsSubCollection)
	{
		Staging.Path = SubCollection.ToSoftObjectPath();
		if (bRecursive && SubCollection.LoadSynchronous()) { SubCollection.Get()->RebuildStagingData(true); }
		Super::UpdateStaging(OwningCollection, bRecursive);
		return;
	}

	Staging.Path = StaticMesh.ToSoftObjectPath();

	const UStaticMesh* M = StaticMesh.LoadSynchronous();
	PCGExAssetCollection::UpdateStagingBounds(Staging, M);

	Super::UpdateStaging(OwningCollection, bRecursive);
}

void FPCGExMeshCollectionEntry::SetAssetPath(const FSoftObjectPath& InPath)
{
	StaticMesh = TSoftObjectPtr<UStaticMesh>(InPath);
	ISMDescriptor.StaticMesh = StaticMesh;
}

#if WITH_EDITOR
void FPCGExMeshCollectionEntry::EDITOR_Sanitize()
{
	FPCGExAssetCollectionEntry::EDITOR_Sanitize();
	if (!bIsSubCollection)
	{
		if (StaticMesh) { ISMDescriptor.StaticMesh = StaticMesh; }
		else if (ISMDescriptor.StaticMesh && !StaticMesh) { StaticMesh = ISMDescriptor.StaticMesh; }
	}
}
#endif

void FPCGExMeshCollectionEntry::OnSubCollectionLoaded()
{
	SubCollectionPtr = Cast<UPCGExMeshCollection>(BaseSubCollectionPtr);
}

#if WITH_EDITOR
void UPCGExMeshCollection::EDITOR_RefreshDisplayNames()
{
	Super::EDITOR_RefreshDisplayNames();
	for (FPCGExMeshCollectionEntry& Entry : Entries)
	{
		Entry.DisplayName = Entry.bIsSubCollection ? FName(TEXT("[") + Entry.SubCollection.GetAssetName() + TEXT("]")) : FName(Entry.ISMDescriptor.StaticMesh.GetAssetName());
	}
}
#endif

void UPCGExMeshCollection::GetAssetPaths(TSet<FSoftObjectPath>& OutPaths, const PCGExAssetCollection::ELoadingFlags Flags) const
{
	const bool bCollectionOnly = Flags == PCGExAssetCollection::ELoadingFlags::RecursiveCollectionsOnly;
	const bool bRecursive = bCollectionOnly || Flags == PCGExAssetCollection::ELoadingFlags::Recursive;

	for (const FPCGExMeshCollectionEntry& Entry : Entries)
	{
		if (Entry.bIsSubCollection)
		{
			if (bRecursive || bCollectionOnly)
			{
				if (const UPCGExMeshCollection* SubCollection = Entry.SubCollection.LoadSynchronous())
				{
					SubCollection->GetAssetPaths(OutPaths, Flags);
				}
			}
			continue;
		}

		if (bCollectionOnly) { continue; }

		OutPaths.Add(Entry.ISMDescriptor.StaticMesh.ToSoftObjectPath());

		for (int i = 0; i < Entry.ISMDescriptor.OverrideMaterials.Num(); i++)
		{
			if (!Entry.ISMDescriptor.OverrideMaterials[i].IsNull())
			{
				OutPaths.Add(Entry.ISMDescriptor.OverrideMaterials[i].ToSoftObjectPath());
			}
		}

		for (int i = 0; i < Entry.ISMDescriptor.RuntimeVirtualTextures.Num(); i++)
		{
			if (!Entry.ISMDescriptor.RuntimeVirtualTextures[i].IsNull())
			{
				OutPaths.Add(Entry.ISMDescriptor.RuntimeVirtualTextures[i].ToSoftObjectPath());
			}
		}
	}
}
