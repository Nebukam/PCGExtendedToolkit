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

	if (Context.CurrentPointIndex == -200)
	{
		// Success!
		// Clean entry idx attribute from output data
		if (OutPointData) { OutPointData->Metadata->DeleteAttribute(PCGExStaging::Tag_EntryIdx); }
		return true;
	}

	if (Context.CurrentPointIndex == -404)
	{
		// Something failed!
		return true;
	}

	if (Context.CurrentPointIndex == -1)
	{
		// Doing async work
		return false;
	}

	if (Context.CurrentPointIndex == 0)
	{
		// Basic initialization, no matter what engine version we're on
		if (!InPointData)
		{
			PCGE_LOG_C(Error, GraphAndLog, &Context, FTEXT( "Missing input data"));
			return true;
		}

		if (!InPointData->Metadata)
		{
			PCGE_LOG_C(Error, GraphAndLog, &Context, FTEXT( "Unable to get metadata from input"));
			return true;
		}

		if (!InPointData->Metadata->GetConstTypedAttribute<int64>(PCGExStaging::Tag_EntryIdx))
		{
			PCGE_LOG_C(Error, GraphAndLog, &Context, FTEXT( "Unable to get hash attribute from input"));
			return true;
		}

		TArray<FPCGMeshInstanceList>* InstanceListPtr = &OutMeshInstances;

		if (OutPointData)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExMeshSelectorStaged::SetupOutPointData);

			const int32 NumPoints = InPointData->GetNumPoints();
			OutPointData->SetNumPoints(NumPoints);
			InPointData->CopyPointsTo(OutPointData, 0, 0, InPointData->GetNumPoints());

			OutPointData->Metadata->DeleteAttribute(PCGExStaging::Tag_EntryIdx);
		}

		auto Exit = [&](const bool bSuccess) { Context.CurrentPointIndex = bSuccess ? -200 : -404; };

		//  3- Build collection map from override attribute set		
		TSharedPtr<PCGExStaging::TPickUnpacker<UPCGExMeshCollection, FPCGExMeshCollectionEntry>> CollectionMap =
			MakeShared<PCGExStaging::TPickUnpacker<UPCGExMeshCollection, FPCGExMeshCollectionEntry>>();

		CollectionMap->UnpackPin(&Context, PCGPinConstants::DefaultParamsLabel);

		if (!CollectionMap->HasValidMapping())
		{
			PCGE_LOG_C(Error, GraphAndLog, &Context, FTEXT( "Unable to find Staging Map data in overrides"));
			Exit(false);
			return true;
		}

		// 2- Partition points

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExMeshSelectorStaged::FindPartitions);

			// TODO : Revisit this to leverage context' AttributeOverridePartition
			if (!CollectionMap->BuildPartitions(InPointData))
			{
				PCGE_LOG_C(Error, GraphAndLog, &Context, FTEXT( "Unable to build any partitions"));
				return true;
			}
		}

		// 3- Select & apply entries for each partition

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExMeshSelectorStaged::SelectEntries);

			const TConstPCGValueRange<FTransform> InTransforms = InPointData->GetConstTransformValueRange();

			for (const TPair<int64, TSharedPtr<TArray<int32>>>& Partition : CollectionMap->HashedPartitions)
			{
				const FPCGExMeshCollectionEntry* Entry = nullptr;
				int16 MaterialPick = -1;

				if (!CollectionMap->ResolveEntry(Partition.Key, Entry, MaterialPick)) { continue; }

				const TArray<int32>& Indices = *Partition.Value.Get();
				TArray<FTransform> Instances;
				const int32 PartitionSize = Indices.Num();
				PCGEx::InitArray(Instances, PartitionSize);

				for (int i = 0; i < PartitionSize; i++) { Instances[i] = InTransforms[Indices[i]]; }

				FPCGSoftISMComponentDescriptor TemplateDescriptor = FPCGSoftISMComponentDescriptor();
				Entry->InitPCGSoftISMDescriptor(TemplateDescriptor);

				//

				if (MaterialPick != -1)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExMeshSelectorStaged::RetrieveMaterials);

					if (Entry->MaterialVariants == EPCGExMaterialVariantsMode::None) { continue; }
					if (Entry->MaterialVariants == EPCGExMaterialVariantsMode::Single)
					{
						TemplateDescriptor.OverrideMaterials.SetNum(Entry->SlotIndex + 1);
						TemplateDescriptor.OverrideMaterials[Entry->SlotIndex] = Entry->MaterialOverrideVariants[MaterialPick].Material;
					}
					else if (Entry->MaterialVariants == EPCGExMaterialVariantsMode::Multi)
					{
						const FPCGExMaterialOverrideCollection& MEntry = Entry->MaterialOverrideVariantsList[MaterialPick];

						const int32 HiIndex = MEntry.GetHighestIndex();
						TemplateDescriptor.OverrideMaterials.SetNum(HiIndex == -1 ? 1 : HiIndex + 1);

						for (int i = 0; i < MEntry.Overrides.Num(); i++)
						{
							const FPCGExMaterialOverrideEntry& SlotEntry = MEntry.Overrides[i];
							const int32 SlotIndex = SlotEntry.SlotIndex == -1 ? 0 : SlotEntry.SlotIndex;
							TemplateDescriptor.OverrideMaterials[SlotIndex] = SlotEntry.Material;
						}
					}
				}

				FPCGMeshInstanceList& NewInstanceList = InstanceListPtr->Emplace_GetRef(TemplateDescriptor);
				NewInstanceList.Descriptor = TemplateDescriptor;
				NewInstanceList.PointData = InPointData;
				NewInstanceList.Instances = Instances;
			}
		}

		Exit(true);
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
