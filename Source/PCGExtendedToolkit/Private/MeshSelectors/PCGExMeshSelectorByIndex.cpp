// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "MeshSelectors/PCGExMeshSelectorByIndex.h"

#include "PCGExMath.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Elements/PCGStaticMeshSpawnerContext.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"
#include "Metadata/PCGObjectPropertyOverride.h"
#include "Metadata/PCGMetadataPartitionCommon.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "Engine/StaticMesh.h"
#include "MeshSelectors/PCGExMeshCollection.h"

#define LOCTEXT_NAMESPACE "PCGMeshSelectorByAttribute"

bool UPCGExMeshSelectorByIndex::Execute(
	FPCGStaticMeshSpawnerContext& Context,
	const UPCGStaticMeshSpawnerSettings* Settings,
	const UPCGPointData* InPointData,
	TArray<FPCGMeshInstanceList>& OutMeshInstances,
	UPCGPointData* OutPointData,
	TArray<FPCGPoint>* OutPoints,
	FPCGMetadataAttribute<FString>* OutAttributeId) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGStaticMeshSpawnerElement::Execute::SelectEntries);

	const TArray<FPCGPoint>& Points = InPointData->GetPoints();
	const int32 LastEntryIndex = MainCollectionPtr->Entries.Num() - 1;

	// Collection is empty. Not an error in itself.
	if (LastEntryIndex < 0) { return true; }

	// Assign points to entries
	int32 CurrentPointIndex = Context.CurrentPointIndex;

	int32 LastCheckpointIndex = CurrentPointIndex;
	constexpr int32 TimeSlicingCheckFrequency = 1024;
	TMap<TSoftObjectPtr<UStaticMesh>, PCGMetadataValueKey>& MeshToValueKey = Context.MeshToValueKey;

	while (CurrentPointIndex < Points.Num())
	{
		const FPCGPoint& Point = Points[CurrentPointIndex++];
		const int32 PickedIndex = PCGExMath::SanitizeIndex(CurrentPointIndex - 1, LastEntryIndex, IndexSafety);

		if (PickedIndex == -1)
		{
			// Invalid index
			continue;
		}

		const FPCGExMeshCollectionEntry& Entry = MainCollectionPtr->Entries[PickedIndex];
		const bool bNeedsReverseCulling = (Point.Transform.GetDeterminant() < 0);

		//bUseAttributeMaterialOverrides, MaterialOverrideHelper.GetMaterialOverrides(Point.MetadataEntry)
		FPCGMeshInstanceList& InstanceList = GetInstanceList(OutMeshInstances, Entry, bNeedsReverseCulling);

		InstanceList.Instances.Emplace(Point.Transform);
		InstanceList.InstancesMetadataEntry.Emplace(Point.MetadataEntry);

		const TSoftObjectPtr<UStaticMesh>& Mesh = InstanceList.Descriptor.StaticMesh;

		if (OutPoints && OutAttributeId)
		{
			FPCGPoint& OutPoint = OutPoints->Add_GetRef(Point);
			PCGMetadataValueKey* OutValueKey = MeshToValueKey.Find(Mesh);
			if (!OutValueKey)
			{
				PCGMetadataValueKey ValueKey = OutAttributeId->AddValue(Mesh.ToSoftObjectPath().ToString());
				OutValueKey = &MeshToValueKey.Add(Mesh, ValueKey);
			}

			check(OutValueKey);

			OutPointData->Metadata->InitializeOnSet(OutPoint.MetadataEntry);
			OutAttributeId->SetValueFromValueKey(OutPoint.MetadataEntry, *OutValueKey);

			if (Settings->bApplyMeshBoundsToPoints)
			{
				TArray<int32>& PointIndices = Context.MeshToOutPoints.FindOrAdd(Mesh).FindOrAdd(OutPointData);
				// CurrentPointIndex - 1, because CurrentPointIndex is incremented at the beginning of the loop
				PointIndices.Emplace(CurrentPointIndex - 1);
			}
		}

		// Check if we should stop here and continue in a subsequent call
		if (CurrentPointIndex - LastCheckpointIndex >= TimeSlicingCheckFrequency)
		{
			if (Context.ShouldStop())
			{
				return false;
			}
			else
			{
				LastCheckpointIndex = CurrentPointIndex;
			}
		}
	}

	Context.CurrentPointIndex = CurrentPointIndex;
	return true;
}

#undef LOCTEXT_NAMESPACE
