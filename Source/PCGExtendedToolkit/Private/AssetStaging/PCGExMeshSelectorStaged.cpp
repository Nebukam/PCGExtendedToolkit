// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetStaging/PCGExMeshSelectorStaged.h"

#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Elements/PCGStaticMeshSpawnerContext.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"
#include "Metadata/PCGObjectPropertyOverride.h"
#include "Metadata/PCGMetadataPartitionCommon.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "Algo/AnyOf.h"
#include "Collections/PCGExStaging.h"
#include "Engine/StaticMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGExMeshSelectorStaged)

#define LOCTEXT_NAMESPACE "PCGExMeshSelectorStaged"

namespace PCGMeshSelectorAttribute
{
	// Returns variation based on mesh, material overrides and reverse culling
	FPCGMeshInstanceList& GetInstanceList(
		TArray<FPCGMeshInstanceList>& InstanceLists,
		const FPCGSoftISMComponentDescriptor& TemplateDescriptor,
		TSoftObjectPtr<UStaticMesh> Mesh,
		const TArray<TSoftObjectPtr<UMaterialInterface>>& MaterialOverrides,
		bool bReverseCulling,
		const UPCGPointData* InPointData,
		const int AttributePartitionIndex = INDEX_NONE)
	{
		for (FPCGMeshInstanceList& InstanceList : InstanceLists)
		{
			if (InstanceList.Descriptor.StaticMesh == Mesh &&
				InstanceList.Descriptor.bReverseCulling == bReverseCulling &&
				InstanceList.Descriptor.OverrideMaterials == MaterialOverrides &&
				InstanceList.AttributePartitionIndex == AttributePartitionIndex)
			{
				return InstanceList;
			}
		}

		FPCGMeshInstanceList& NewInstanceList = InstanceLists.Emplace_GetRef(TemplateDescriptor);
		NewInstanceList.Descriptor.StaticMesh = Mesh;
		NewInstanceList.Descriptor.OverrideMaterials = MaterialOverrides;
		NewInstanceList.Descriptor.bReverseCulling = bReverseCulling;
		NewInstanceList.AttributePartitionIndex = AttributePartitionIndex;
		NewInstanceList.PointData = InPointData;

		return NewInstanceList;
	}
}

bool UPCGExMeshSelectorStaged::SelectInstances(
	FPCGStaticMeshSpawnerContext& Context,
	const UPCGStaticMeshSpawnerSettings* Settings,
	const UPCGPointData* InPointData,
	TArray<FPCGMeshInstanceList>& OutMeshInstances,
	UPCGPointData* OutPointData) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExMeshSelectorStaged::SelectInstances);

	if (!InPointData)
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, LOCTEXT("InputMissingData", "Missing input data"));
		return true;
	}

	if (!InPointData->Metadata)
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, LOCTEXT("InputMissingMetadata", "Unable to get metadata from input"));
		return true;
	}

	FPCGMeshMaterialOverrideHelper& MaterialOverrideHelper = Context.MaterialOverrideHelper;
	if (!MaterialOverrideHelper.IsInitialized())
	{
		//MaterialOverrideHelper.Initialize(Context, bUseAttributeMaterialOverrides, TemplateDescriptor.OverrideMaterials, MaterialOverrideAttributes, InPointData->Metadata);
	}

	if (!MaterialOverrideHelper.IsValid())
	{
		return true;
	}

	if (Context.CurrentPointIndex == 0 && OutPointData)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExMeshSelectorStaged::SetupOutPointData);
		OutPointData->SetPoints(InPointData->GetPoints());
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExMeshSelectorStaged::SelectEntries);

	// Assign points to entries
	int32 CurrentPartitionIndex = Context.CurrentPointIndex; // misnomer but we're reusing another concept from the context
	const TArray<FPCGPoint>& Points = InPointData->GetPoints();
	const int32 NumPoints = Points.Num();
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExMeshSelectorStaged::SelectEntries::PushingPointsToInstanceLists);

		// TODO: Revisit this when attribute partitioning is returned in a more optimized form
		// The partition index is used to assign the point to the correct partition's instance
		while (CurrentPartitionIndex < NumPoints)
		{
			// TODO : Get collection mapping from Overrides pin, find a suitable collection. If there are multiple, throw.
			// Get per-point Idx from PCGExStaging::Tag_EntryIdx
			// Build a temp PCGExStaging::TCollectionPickDatasetUnpacker; a bit unelegant but will do as it stores very little data

			/*
			if (MaterialOverrideHelper.OverridesMaterials())
			{
				for (int32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
				{
					const FPCGPoint& Point = Points[PointIndices[PointIndex]];
					FPCGMeshInstanceList& InstanceList = PCGMeshSelectorAttribute::GetInstanceList(OutMeshInstances, CurrentPartitionDescriptor, CurrentPartitionDescriptor.StaticMesh, MaterialOverrideHelper.GetMaterialOverrides(Point.MetadataEntry), bReverseTransform, InPointData, PartitionIndex);
					InstanceList.Instances.Emplace(Point.Transform);
					InstanceList.InstancesIndices.Emplace(PointIndex);
				}
			}
			else
			{
				TArray<TSoftObjectPtr<UMaterialInterface>> DummyMaterialList;
				FPCGMeshInstanceList& InstanceList = PCGMeshSelectorAttribute::GetInstanceList(OutMeshInstances, CurrentPartitionDescriptor, CurrentPartitionDescriptor.StaticMesh, DummyMaterialList, bReverseTransform, InPointData, PartitionIndex);

				check(InstanceList.Instances.Num() == InstanceList.InstancesIndices.Num());
				const int32 InstanceOffset = InstanceList.Instances.Num();
				InstanceList.Instances.SetNum(InstanceOffset + PointIndices.Num());
				InstanceList.InstancesIndices.Append(PointIndices);

				for (int32 PointIndex = 0; PointIndex < PointIndices.Num(); ++PointIndex)
				{
					const FPCGPoint& Point = Points[PointIndices[PointIndex]];
					InstanceList.Instances[InstanceOffset + PointIndex] = Point.Transform;
				}
			}

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExMeshSelectorStaged::SelectEntries::AddPointsToInstanceList);
				AddPointsToInstanceList(Partition, false);
				AddPointsToInstanceList(ReverseInstances, true);
			}

			if (Context.ShouldStop())
			{
				break;
			}
			*/
		}
	}

	Context.CurrentPointIndex = CurrentPartitionIndex; // misnomer, but we're using the same context
	return CurrentPartitionIndex == Context.AttributeOverridePartition.Num();
}

#undef LOCTEXT_NAMESPACE
