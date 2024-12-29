// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Collections/PCGExMeshCollection.h"

void FPCGExMaterialOverrideCollection::GetAssetPaths(TSet<FSoftObjectPath>& OutPaths) const
{
	for (const FPCGExMaterialOverrideEntry& Entry : Overrides) { OutPaths.Add(Entry.Material.ToSoftObjectPath()); }
}

void FPCGExMeshCollectionEntry::GetAssetPaths(TSet<FSoftObjectPath>& OutPaths) const
{
	FPCGExAssetCollectionEntry::GetAssetPaths(OutPaths);

	// Override materials

	switch (MaterialVariants)
	{
	default:
	case EPCGExMaterialVariantsMode::None:
		break;
	case EPCGExMaterialVariantsMode::Single:
		for (const FPCGExMaterialOverrideSingleEntry& Entry : MaterialOverrideVariants) { OutPaths.Add(Entry.Material.ToSoftObjectPath()); }
		break;
	case EPCGExMaterialVariantsMode::Multi:
		for (const FPCGExMaterialOverrideCollection& Entry : MaterialOverrideVariantsList) { Entry.GetAssetPaths(OutPaths); }
		break;
	}

	// ISM

	for (int i = 0; i < ISMDescriptor.OverrideMaterials.Num(); i++)
	{
		if (!ISMDescriptor.OverrideMaterials[i].IsNull())
		{
			OutPaths.Add(ISMDescriptor.OverrideMaterials[i].ToSoftObjectPath());
		}
	}

	for (int i = 0; i < ISMDescriptor.RuntimeVirtualTextures.Num(); i++)
	{
		if (!ISMDescriptor.RuntimeVirtualTextures[i].IsNull())
		{
			OutPaths.Add(ISMDescriptor.RuntimeVirtualTextures[i].ToSoftObjectPath());
		}
	}

	// SM

	for (int i = 0; i < SMDescriptor.OverrideMaterials.Num(); i++)
	{
		if (!SMDescriptor.OverrideMaterials[i].IsNull())
		{
			OutPaths.Add(SMDescriptor.OverrideMaterials[i].ToSoftObjectPath());
		}
	}

	for (int i = 0; i < SMDescriptor.RuntimeVirtualTextures.Num(); i++)
	{
		if (!SMDescriptor.RuntimeVirtualTextures[i].IsNull())
		{
			OutPaths.Add(SMDescriptor.RuntimeVirtualTextures[i].ToSoftObjectPath());
		}
	}
}

bool FPCGExMeshCollectionEntry::Validate(const UPCGExAssetCollection* ParentCollection)
{
	if (!bIsSubCollection)
	{
		if (!StaticMesh.ToSoftObjectPath().IsValid() && ParentCollection->bDoNotIgnoreInvalidEntries) { return false; }
	}

	return Super::Validate(ParentCollection);
}

#if WITH_EDITOR
void FPCGExMeshCollectionEntry::EDITOR_Sanitize()
{
	FPCGExAssetCollectionEntry::EDITOR_Sanitize();

	if (!bIsSubCollection)
	{
		InternalSubCollection = nullptr;
		if (StaticMesh) { ISMDescriptor.StaticMesh = StaticMesh; }
		//else if (ISMDescriptor.StaticMesh && !StaticMesh) { StaticMesh = ISMDescriptor.StaticMesh; }
	}
	else
	{
		InternalSubCollection = SubCollection;
	}
}
#endif

void FPCGExMeshCollectionEntry::UpdateStaging(const UPCGExAssetCollection* OwningCollection, const int32 InInternalIndex, const bool bRecursive)
{
	if (bIsSubCollection)
	{
		Super::UpdateStaging(OwningCollection, InInternalIndex, bRecursive);
		return;
	}

	if (Staging.InternalIndex == -1 && GetDefault<UPCGExGlobalSettings>()->bDisableCollisionByDefault)
	{
		ISMDescriptor.BodyInstance.SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
		SMDescriptor.BodyInstance.SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	}

	Staging.Path = StaticMesh.ToSoftObjectPath();

	if (MaterialVariants != EPCGExMaterialVariantsMode::None)
	{
		MaterialVariantsCumulativeWeight = -1;
		MaterialVariantsOrder.Reset();
		MaterialVariantsWeights.Reset();

		if (MaterialVariants == EPCGExMaterialVariantsMode::Single)
		{
			MaterialVariantsOrder.Reserve(MaterialOverrideVariants.Num());
			MaterialVariantsWeights.Reserve(MaterialOverrideVariants.Num());

			for (int i = 0; i < MaterialOverrideVariants.Num(); i++)
			{
#if WITH_EDITOR
				FPCGExMaterialOverrideSingleEntry& MEntry = MaterialOverrideVariants[i];
				MEntry.UpdateDisplayName();
#else
				const FPCGExMaterialOverrideSingleEntry& MEntry = MaterialOverrideVariants[i];
#endif
				MaterialVariantsOrder.Add(i);
				MaterialVariantsWeights.Add(MEntry.Weight);
			}

			MaterialVariantsOrder.Sort([&](const int32 A, const int32 B) { return MaterialOverrideVariants[A].Weight < MaterialOverrideVariants[B].Weight; });
			MaterialVariantsWeights.Sort([&](const int32 A, const int32 B) { return A < B; });
		}
		else
		{
			MaterialVariantsOrder.Reserve(MaterialOverrideVariantsList.Num());
			MaterialVariantsWeights.Reserve(MaterialOverrideVariantsList.Num());

			for (int i = 0; i < MaterialOverrideVariantsList.Num(); i++)
			{
#if WITH_EDITOR
				FPCGExMaterialOverrideCollection& MEntry = MaterialOverrideVariantsList[i];
				MEntry.UpdateDisplayName();
#else
				const FPCGExMaterialOverrideCollection& MEntry = MaterialOverrideVariantsList[i];
#endif
				MaterialVariantsOrder.Add(i);
				MaterialVariantsWeights.Add(MEntry.Weight);
			}

			MaterialVariantsOrder.Sort([&](const int32 A, const int32 B) { return MaterialOverrideVariantsList[A].Weight < MaterialOverrideVariantsList[B].Weight; });
			MaterialVariantsWeights.Sort([&](const int32 A, const int32 B) { return A < B; });
		}
	}

	const UStaticMesh* M = PCGExHelpers::LoadBlocking_AnyThread(StaticMesh);
	PCGExAssetCollection::UpdateStagingBounds(Staging, M);

	Super::UpdateStaging(OwningCollection, InInternalIndex, bRecursive);
}

void FPCGExMeshCollectionEntry::SetAssetPath(const FSoftObjectPath& InPath)
{
	StaticMesh = TSoftObjectPtr<UStaticMesh>(InPath);
	ISMDescriptor.StaticMesh = StaticMesh;
}

#if PCGEX_ENGINE_VERSION > 504
void FPCGExMeshCollectionEntry::InitPCGSoftISMDescriptor(FPCGSoftISMComponentDescriptor& TargetDescriptor) const
{
	PCGExHelpers::CopyStructProperties(
		&ISMDescriptor,
		&TargetDescriptor,
		FSoftISMComponentDescriptor::StaticStruct(),
		FPCGSoftISMComponentDescriptor::StaticStruct());
}
#endif

#if WITH_EDITOR
void UPCGExMeshCollection::EDITOR_RefreshDisplayNames()
{
	Super::EDITOR_RefreshDisplayNames();
	for (FPCGExMeshCollectionEntry& Entry : Entries)
	{
		FString DisplayName = Entry.bIsSubCollection ? TEXT("[") + Entry.SubCollection.GetName() + TEXT("]") : Entry.StaticMesh.GetAssetName();
		DisplayName += FString::Printf(TEXT(" @ %d "), Entry.Weight);
		Entry.DisplayName = FName(DisplayName);
	}
}

void UPCGExMeshCollection::EDITOR_DisableCollisions()
{
	Modify(true);

	for (FPCGExMeshCollectionEntry& Entry : Entries)
	{
		Entry.ISMDescriptor.BodyInstance.SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
		Entry.SMDescriptor.BodyInstance.SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	}

	FPropertyChangedEvent EmptyEvent(nullptr);
	PostEditChangeProperty(EmptyEvent);
	MarkPackageDirty();
}

#endif
