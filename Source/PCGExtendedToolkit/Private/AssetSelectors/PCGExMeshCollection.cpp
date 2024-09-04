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

void FPCGExMeshCollectionEntry::UpdateStaging(const UPCGExAssetCollection* OwningCollection, const bool bRecursive)
{
	if (bIsSubCollection)
	{		
		Staging.Path = SubCollection.ToSoftObjectPath();
		if (bRecursive && SubCollection.LoadSynchronous()) { SubCollection.Get()->RebuildStagingData(true); }
		Super::UpdateStaging(OwningCollection, bRecursive);
		return;
	}

	Staging.Path = Descriptor.StaticMesh.ToSoftObjectPath();

	const UStaticMesh* M = Descriptor.StaticMesh.LoadSynchronous();
	PCGExAssetCollection::UpdateStagingBounds(Staging, M);

	Super::UpdateStaging(OwningCollection, bRecursive);
}

void FPCGExMeshCollectionEntry::SetAssetPath(FSoftObjectPath InPath)
{
	Descriptor.StaticMesh = TSoftObjectPtr<UStaticMesh>(InPath);
}

void FPCGExMeshCollectionEntry::OnSubCollectionLoaded()
{
	SubCollectionPtr = Cast<UPCGExMeshCollection>(BaseSubCollectionPtr);
}

void UPCGExMeshCollection::RebuildStagingData(const bool bRecursive)
{
	for (FPCGExMeshCollectionEntry& Entry : Entries) { Entry.UpdateStaging(this, bRecursive); }
	Super::RebuildStagingData(bRecursive);
}

#if WITH_EDITOR
void UPCGExMeshCollection::EDITOR_RefreshDisplayNames()
{
	Super::EDITOR_RefreshDisplayNames();
	for (FPCGExMeshCollectionEntry& Entry : Entries)
	{
		Entry.DisplayName = Entry.bIsSubCollection ? FName(TEXT("[") + Entry.SubCollection.GetAssetName() + TEXT("]")) : FName(Entry.Descriptor.StaticMesh.GetAssetName());
	}
}
#endif

UPCGExAssetCollection* UPCGExMeshCollection::GetCollectionFromAttributeSet(const FPCGContext* InContext, const UPCGParamData* InAttributeSet, const FPCGExAssetAttributeSetDetails& Details, const bool bBuildStaging) const
{
	return GetCollectionFromAttributeSetTpl<UPCGExMeshCollection>(InContext, InAttributeSet, Details, bBuildStaging);
}

UPCGExAssetCollection* UPCGExMeshCollection::GetCollectionFromAttributeSet(const FPCGContext* InContext, const FName InputPin, const FPCGExAssetAttributeSetDetails& Details, const bool bBuildStaging) const
{
	return GetCollectionFromAttributeSetTpl<UPCGExMeshCollection>(InContext, InputPin, Details, bBuildStaging);
}

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

		OutPaths.Add(Entry.Descriptor.StaticMesh.ToSoftObjectPath());

		for (int i = 0; i < Entry.Descriptor.OverrideMaterials.Num(); ++i)
		{
			if (!Entry.Descriptor.OverrideMaterials[i].IsNull())
			{
				OutPaths.Add(Entry.Descriptor.OverrideMaterials[i].ToSoftObjectPath());
			}
		}

		for (int i = 0; i < Entry.Descriptor.RuntimeVirtualTextures.Num(); ++i)
		{
			if (!Entry.Descriptor.RuntimeVirtualTextures[i].IsNull())
			{
				OutPaths.Add(Entry.Descriptor.RuntimeVirtualTextures[i].ToSoftObjectPath());
			}
		}
	}
}

void UPCGExMeshCollection::BuildCache()
{
	Super::BuildCache(Entries);
}
