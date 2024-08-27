// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetSelectors/PCGExMeshSelectorByIndex.h"

#include "PCGExMath.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "AssetSelectors/PCGExMeshCollection.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Elements/PCGStaticMeshSpawnerContext.h"

#include "Engine/StaticMesh.h"

#define LOCTEXT_NAMESPACE "PCGMeshSelectorByIndex"

bool UPCGExMeshSelectorByIndex::Execute(PCGExMeshSelection::FCtx& Ctx) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGStaticMeshSpawnerElement::Execute::SelectEntries);

	const TArray<FPCGPoint>& Points = Ctx.InPointData->GetPoints();
	const int32 LastEntryIndex = MainCollectionPtr->GetValidEntryNum() - 1;

	// Collection is empty. Not an error in itself.
	if (LastEntryIndex < 0)
	{
		Ctx.Context->CurrentPointIndex = Points.Num();
		return true;
	}

	// Assign points to entries
	int32 CurrentPointIndex = Ctx.Context->CurrentPointIndex;

	int32 LastCheckpointIndex = CurrentPointIndex;
	TMap<TSoftObjectPtr<UStaticMesh>, PCGMetadataValueKey>& MeshToValueKey = Ctx.Context->MeshToValueKey;

	while (CurrentPointIndex < Points.Num())
	{
		const FPCGPoint& Point = Points[CurrentPointIndex++];
		const int32 DesiredIndex = PCGExMath::SanitizeIndex(CurrentPointIndex - 1, LastEntryIndex, IndexSafety);

		if (DesiredIndex == -1)
		{
			// Invalid pick
			continue;
		}


		FPCGExMeshCollectionEntry Entry = FPCGExMeshCollectionEntry{};
		
		MainCollectionPtr->GetEntry(
			Entry, MainCollectionPtr->Entries, DesiredIndex,
			PCGExRandom::GetSeedFromPoint(Point, LocalSeed, Ctx.Settings, Ctx.Context->SourceComponent.Get()));

		if (Entry.bBadEntry) { continue; }

		//bUseAttributeMaterialOverrides, MaterialOverrideHelper.GetMaterialOverrides(Point.MetadataEntry)

		// CurrentPointIndex - 1, because CurrentPointIndex is incremented at the beginning of the loop
		RegisterPick(Entry, Point, CurrentPointIndex - 1, Ctx);

		// Check if we should stop here and continue in a subsequent call
		if (CurrentPointIndex - LastCheckpointIndex >= TimeSlicingCheckFrequency)
		{
			if (Ctx.Context->ShouldStop()) { return false; }
			LastCheckpointIndex = CurrentPointIndex;
		}
	}

	Ctx.Context->CurrentPointIndex = CurrentPointIndex;
	return true;
}

#undef LOCTEXT_NAMESPACE
