// Copyright 2024 Timothé Lapetite and contributors
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

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGExMeshSelectorStaged)

#define LOCTEXT_NAMESPACE "PCGExMeshSelectorStaged"

namespace PCGExMeshSelectorStaged
{
	// Returns variation based on mesh, material overrides and reverse culling
	FPCGMeshInstanceList& GetInstanceList(
		TArray<FPCGMeshInstanceList>& InstanceLists,
		const FPCGExMeshCollectionEntry* Entry,
		const TArray<TSoftObjectPtr<UMaterialInterface>>& MaterialOverrides,
		const UPCGPointData* InPointData)
	{
		for (FPCGMeshInstanceList& InstanceList : InstanceLists)
		{
			if (InstanceList.Descriptor == Entry->ISMDescriptor)
			{
				return InstanceList;
			}
		}

#if PCGEX_ENGINE_VERSION > 504
		FPCGSoftISMComponentDescriptor TemplateDescriptor = FPCGSoftISMComponentDescriptor();
		Entry->InitPCGSoftISMDescriptor(TemplateDescriptor);

		FPCGMeshInstanceList& NewInstanceList = InstanceLists.Emplace_GetRef(TemplateDescriptor);
		NewInstanceList.Descriptor = TemplateDescriptor;
		NewInstanceList.PointData = InPointData;
#else
		FSoftISMComponentDescriptor TemplateDescriptor = FSoftISMComponentDescriptor(Entry->ISMDescriptor);
		FPCGMeshInstanceList& NewInstanceList = InstanceLists.Emplace_GetRef(TemplateDescriptor);
		NewInstanceList.Descriptor = TemplateDescriptor;
#endif

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
		PCGE_LOG_C(Error, GraphAndLog, &Context, FTEXT( "Missing input data"));
		return true;
	}

	if (!InPointData->Metadata)
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, FTEXT( "Unable to get metadata from input"));
		return true;
	}

	const FPCGMetadataAttribute<int64>* HashAttribute = InPointData->Metadata->GetConstTypedAttribute<int64>(PCGExStaging::Tag_EntryIdx);

	if (!HashAttribute)
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, FTEXT( "Unable to get hash attribute from input"));
		return true;
	}

	TSharedPtr<PCGExStaging::TPickUnpacker<UPCGExMeshCollection, FPCGExMeshCollectionEntry>> CollectionMap =
		MakeShared<PCGExStaging::TPickUnpacker<UPCGExMeshCollection, FPCGExMeshCollectionEntry>>();

	CollectionMap->UnpackPin(&Context, PCGPinConstants::DefaultParamsLabel);

	if (!CollectionMap->HasValidMapping())
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, FTEXT( "Unable to find Staging Map data in overrides"));
		return true;
	}

	FPCGMeshMaterialOverrideHelper& MaterialOverrideHelper = Context.MaterialOverrideHelper;
	if (!MaterialOverrideHelper.IsInitialized())
	{
		//MaterialOverrideHelper.Initialize(Context, bUseAttributeMaterialOverrides, TemplateDescriptor.OverrideMaterials, MaterialOverrideAttributes, InPointData->Metadata);
	}

	if (!MaterialOverrideHelper.IsValid())
	{
		//return true;
	}

	if (Context.CurrentPointIndex == 0 && OutPointData)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExMeshSelectorStaged::SetupOutPointData);
		OutPointData->SetPoints(InPointData->GetPoints());
	}

	const TArray<FPCGPoint>& InPoints = InPointData->GetPoints();
	const int32 NumPoints = InPoints.Num();

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExMeshSelectorStaged::SelectEntries);

		while (Context.CurrentPointIndex < NumPoints)
		{
			const FPCGPoint& Point = InPoints[Context.CurrentPointIndex++];

			int64 EntryHash = HashAttribute->GetValueFromItemKey(Point.MetadataEntry);

			const FPCGExMeshCollectionEntry* Entry = nullptr;
			if (!CollectionMap->ResolveEntry(EntryHash, Entry)) { continue; }

			TArray<TSoftObjectPtr<UMaterialInterface>> DummyMaterialList;
			FPCGMeshInstanceList& InstanceList = PCGExMeshSelectorStaged::GetInstanceList(OutMeshInstances, Entry, DummyMaterialList, InPointData);
			InstanceList.Instances.Add(Point.Transform);

			if (Context.ShouldStop()) { break; }
		}
	}

	if (Context.CurrentPointIndex == NumPoints && OutPointData)
	{
		OutPointData->Metadata->DeleteAttribute(PCGExStaging::Tag_EntryIdx);
	}

	return Context.CurrentPointIndex == NumPoints;
}

#undef LOCTEXT_NAMESPACE
