// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetSelectors/PCGExMeshSelectorByWeight.h"

#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Elements/PCGStaticMeshSpawnerContext.h"

#include "Engine/StaticMesh.h"
#include "AssetSelectors/PCGExMeshCollection.h"

#define LOCTEXT_NAMESPACE "PCGMeshSelectorByWeight"

bool UPCGExMeshSelectorByWeight::Execute(PCGExMeshSelection::FCtx& Ctx) const
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

	while (CurrentPointIndex < Points.Num())
	{
		const FPCGPoint& Point = Points[CurrentPointIndex++];

		FPCGExMeshCollectionEntry Entry = FPCGExMeshCollectionEntry{};

		if (!MainCollectionPtr->GetEntryWeightedRandom(
			Entry, MainCollectionPtr->Entries,
			PCGExRandom::GetSeedFromPoint(SeedComponents, Point, LocalSeed, Ctx.Settings, Ctx.Context->SourceComponent.Get())))
		{
			continue;
		}

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
