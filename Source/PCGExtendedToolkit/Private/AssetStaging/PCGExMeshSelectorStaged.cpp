// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetStaging/PCGExMeshSelectorStaged.h"

#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/PCGStaticMeshSpawnerContext.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"

#include "AssetStaging/PCGExStaging.h"
#include "Collections/PCGExMeshCollection.h"
#include "Engine/StaticMesh.h"
#include "Tasks/Task.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGExMeshSelectorStaged)

#define LOCTEXT_NAMESPACE "PCGExMeshSelectorStaged"

namespace PCGExMeshSelectorStaged
{
	// Returns variation based on mesh, material overrides and reverse culling
	FPCGMeshInstanceList& GetInstanceList(
		TArray<FPCGMeshInstanceList>& InstanceLists,
		const FPCGSoftISMComponentDescriptor& TemplateDescriptor,
		TSoftObjectPtr<UStaticMesh> Mesh,
		const TArray<TSoftObjectPtr<UMaterialInterface>>& MaterialOverrides,
		bool bReverseCulling,
		const UPCGBasePointData* InPointData,
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

bool UPCGExMeshSelectorStaged::SelectMeshInstances(FPCGStaticMeshSpawnerContext& Context, const UPCGStaticMeshSpawnerSettings* Settings, const UPCGBasePointData* InPointData, TArray<FPCGMeshInstanceList>& OutMeshInstances, UPCGBasePointData* OutPointData) const
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

	if (!InPointData->Metadata->GetConstTypedAttribute<int64>(PCGExStaging::Tag_EntryIdx))
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, FTEXT( "Unable to get hash attribute from input"));
		return true;
	}

	if (Context.CurrentPointIndex == 0)
	{
		if (OutPointData)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExMeshSelectorStaged::SetupOutPointData);

			const int32 NumPoints = InPointData->GetNumPoints();
			OutPointData->SetNumPoints(NumPoints);
			InPointData->CopyPointsTo(OutPointData, 0, 0, InPointData->GetNumPoints());

			OutPointData->Metadata->DeleteAttribute(PCGExStaging::Tag_EntryIdx);
		}

		// 1- Build collection map from override attribute set		
		TSharedPtr<PCGExStaging::TPickUnpacker<UPCGExMeshCollection, FPCGExMeshCollectionEntry>> CollectionMap =
			MakeShared<PCGExStaging::TPickUnpacker<UPCGExMeshCollection, FPCGExMeshCollectionEntry>>();

		CollectionMap->UnpackPin(&Context, PCGPinConstants::DefaultParamsLabel);

		if (!CollectionMap->HasValidMapping())
		{
			PCGE_LOG_C(Error, GraphAndLog, &Context, FTEXT( "Unable to find Staging Map data in overrides"));
			return true;
		}

		// 2- Partition points
		if (!CollectionMap->BuildPartitions(InPointData, OutMeshInstances))
		{
			PCGE_LOG_C(Error, GraphAndLog, &Context, FTEXT( "Unable to build any partitions"));
			return true;
		}

		// 3- Resolve & apply entries for each partition
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExMeshSelectorStaged::SelectEntries);

			TConstPCGValueRange<FTransform> InTransforms = InPointData->GetConstTransformValueRange();

			for (const TPair<int64, int32>& Partition : CollectionMap->IndexedPartitions)
			{
				const FPCGExMeshCollectionEntry* Entry = nullptr;
				int16 MaterialPick = -1;

				if (!CollectionMap->ResolveEntry(Partition.Key, Entry, MaterialPick)) { continue; }

				FPCGMeshInstanceList& InstanceList = OutMeshInstances[Partition.Value];

				FPCGSoftISMComponentDescriptor& Descriptor = InstanceList.Descriptor;
				Entry->InitPCGSoftISMDescriptor(Descriptor);
				Entry->ApplyMaterials(MaterialPick, Descriptor);

				const TArray<int32>& InstanceIndices = InstanceList.InstancesIndices;
				InstanceList.Instances.Reserve(InstanceIndices.Num());
				for (const int32 i : InstanceIndices) { InstanceList.Instances.Emplace(InTransforms[i]); }
			}
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
