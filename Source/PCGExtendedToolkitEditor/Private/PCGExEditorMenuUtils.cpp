// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExEditorMenuUtils.h"

#include "Collections/PCGExActorCollectionUtils.h"
#include "Collections/PCGExMeshCollectionUtils.h"

#include "Engine/World.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "PCGEditorMenuUtils"

namespace PCGExEditorMenuUtils
{
	FToolMenuSection& CreatePCGExSection(UToolMenu* Menu)
	{
		const FName LevelSectionName = TEXT("PCGEx");
		FToolMenuSection* SectionPtr = Menu->FindSection(LevelSectionName);
		if (!SectionPtr)
		{
			SectionPtr = &(Menu->AddSection(LevelSectionName, LOCTEXT("PCGExSectionLabel", "PCGEx")));
		}

		FToolMenuSection& Section = *SectionPtr;
		return Section;
	}

	void CreateOrUpdatePCGExAssetCollectionsFromMenu(UToolMenu* Menu, TArray<FAssetData>& Assets)
	{
		TArray<FAssetData> TempStaticMeshes;
		TArray<TObjectPtr<UPCGExMeshCollection>> TempMeshCollections;

		TArray<FAssetData> TempActorAssets;
		TArray<TObjectPtr<UPCGExActorCollection>> TempActorCollections;

		for (const FAssetData& Asset : Assets)
		{
			if (Asset.IsInstanceOf<UStaticMesh>())
			{
				TempStaticMeshes.Add(Asset);
			}

			if (Asset.IsInstanceOf<UPCGExMeshCollection>())
			{
				if (UPCGExMeshCollection* Collection = TSoftObjectPtr<UPCGExMeshCollection>(Asset.GetSoftObjectPath()).LoadSynchronous())
				{
					TempMeshCollections.Add(Collection);
				}
			}

			if (Asset.IsInstanceOf<AActor>())
			{
				TempActorAssets.Add(Asset);
			}

			if (Asset.IsInstanceOf<UPCGExActorCollection>())
			{
				if (UPCGExActorCollection* Collection = TSoftObjectPtr<UPCGExActorCollection>(Asset.GetSoftObjectPath()).LoadSynchronous())
				{
					TempActorCollections.Add(Collection);
				}
			}
		}

		if (TempStaticMeshes.IsEmpty() && TempActorAssets.IsEmpty())
		{
			return;
		}

		FToolMenuSection& Section = CreatePCGExSection(Menu);

		if (!TempStaticMeshes.IsEmpty() || !TempActorAssets.IsEmpty())
		{
			FToolUIAction UIAction;
			UIAction.ExecuteAction.BindLambda(
				[
					Meshes = MoveTemp(TempStaticMeshes),
					MeshCollections = MoveTemp(TempMeshCollections),
					Actors = MoveTemp(TempActorAssets),
					ActorCollections = MoveTemp(TempActorCollections)](const FToolMenuContext& MenuContext)
				{
					FScopedSlowTask SlowTask(0.0f, LOCTEXT("CreateOrUpdatePCGExMeshCollection", "Create or Update Asset Collection(s) from selection..."));

					if (MeshCollections.IsEmpty()) { PCGExMeshCollectionUtils::CreateCollectionFrom(Meshes); }
					else { PCGExMeshCollectionUtils::UpdateCollectionsFrom(MeshCollections, Meshes); }

					if (ActorCollections.IsEmpty()) { PCGExActorCollectionUtils::CreateCollectionFrom(Actors); }
					else { PCGExActorCollectionUtils::UpdateCollectionsFrom(ActorCollections, Actors); }
				});

			Section.AddMenuEntry(
				"CreateOrUpdatePCGExMeshCollectionFromMenu",
				TAttribute<FText>(FText::FromString(TEXT("Create or Update Asset Collection(s) from selection"))),
				TAttribute<FText>(FText::FromString(TEXT("If no Asset collection is part of the selection, will create new Mesh and/or Actor collections. If any collection is part of the selection, the selected mesh and/or actor will be added to the selected collection instead."))),
				FSlateIcon(FName("PCGExStyleSet"), "ClassIcon.PCGExAssetCollection"),
				UIAction);
		}
	}
}

#undef LOCTEXT_NAMESPACE
