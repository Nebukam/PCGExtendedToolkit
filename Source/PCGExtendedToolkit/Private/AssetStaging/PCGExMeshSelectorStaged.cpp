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

bool UPCGExMeshSelectorStaged::SelectMeshInstances(FPCGStaticMeshSpawnerContext& Context, const UPCGStaticMeshSpawnerSettings* Settings, const UPCGBasePointData* InPointData, TArray<FPCGMeshInstanceList>& OutMeshInstances, UPCGBasePointData* OutPointData) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExMeshSelectorStaged::SelectInstances);

	PCGE_LOG_C(Error, GraphAndLog, &Context, FTEXT( "PCGEx Mesh Selector Staged is being refactored and is not supported yet for 5.6."));
	
	return true;
	/*
	
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

		// Init material override
		FPCGMeshMaterialOverrideHelper& MaterialOverrideHelper = Context.MaterialOverrideHelper;
		//MaterialOverrideHelper.Initialize(Context, bUseAttributeMaterialOverrides, TemplateDescriptor.OverrideMaterials, MaterialOverrideAttributes, InPointData->Metadata);
		//if (!MaterialOverrideHelper.IsValid()) { return true; }
	}

	TArray<FPCGMeshInstanceList>* InstanceListPtr = &OutMeshInstances;

	if (Context.CurrentPointIndex == 0)
	{
		// Kickstart async work
		Context.CurrentPointIndex = -1;

		TWeakPtr<FPCGContextHandle> CtxHandle = Context.GetOrCreateHandle();
		Context.bIsPaused = true;

		UE::Tasks::Launch(
				TEXT("FactoryInitialization"),
				[CtxHandle, InPointData, InstanceListPtr, OutPointData]()
				{
					if (OutPointData)
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExMeshSelectorStaged::SetupOutPointData);

						const int32 NumPoints = InPointData->GetNumPoints();
						OutPointData->SetNumPoints(NumPoints);
						InPointData->CopyPointsTo(OutPointData, 0, 0, InPointData->GetNumPoints());

						OutPointData->Metadata->DeleteAttribute(PCGExStaging::Tag_EntryIdx);
					}

					const FPCGContext::FSharedContext<FPCGStaticMeshSpawnerContext> SharedContext(CtxHandle);
					FPCGStaticMeshSpawnerContext* Ctx = SharedContext.Get();
					if (!Ctx) { return; }

					auto Exit = [&](const bool bSuccess)
					{
						Ctx->CurrentPointIndex = bSuccess ? -200 : -404;
						Ctx->bIsPaused = false;
					};

					TSharedPtr<PCGExStaging::TPickUnpacker<UPCGExMeshCollection, FPCGExMeshCollectionEntry>> CollectionMap =
						MakeShared<PCGExStaging::TPickUnpacker<UPCGExMeshCollection, FPCGExMeshCollectionEntry>>();

					CollectionMap->UnpackPin(Ctx, PCGPinConstants::DefaultParamsLabel);

					if (!CollectionMap->HasValidMapping())
					{
						PCGE_LOG_C(Error, GraphAndLog, Ctx, FTEXT( "Unable to find Staging Map data in overrides"));
						Exit(false);
						return;
					}

					{
						TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExMeshSelectorStaged::SelectEntries);

						{
							TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExMeshSelectorStaged::FindPartitions);

							if (!CollectionMap->BuildPartitions(InPointData))
							{
								PCGE_LOG_C(Error, GraphAndLog, Ctx, FTEXT( "Unable to build any partitions"));
								Exit(false);
								return;
							}
						}

						const TConstPCGValueRange<FTransform> InTransforms = InPointData->GetConstTransformValueRange();

						{
							TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExMeshSelectorStaged::FillInstances);
							for (const TPair<int64, TSharedPtr<TArray<int32>>>& Partition : CollectionMap->HashedPartitions)
							{
								const FPCGExMeshCollectionEntry* Entry = nullptr;
								if (!CollectionMap->ResolveEntry(Partition.Key, Entry)) { continue; }

								const TArray<int32>& Indices = *Partition.Value.Get();
								TArray<FTransform> Instances;
								const int32 PartitionSize = Indices.Num();
								PCGEx::InitArray(Instances, PartitionSize);

								for (int i = 0; i < PartitionSize; i++) { Instances[i] = InTransforms[Indices[i]]; }

								if (!CtxHandle.IsValid()) { return; }

								FPCGSoftISMComponentDescriptor TemplateDescriptor = FPCGSoftISMComponentDescriptor();
								Entry->InitPCGSoftISMDescriptor(TemplateDescriptor);

								// TODO : 

								FPCGMeshInstanceList& NewInstanceList = InstanceListPtr->Emplace_GetRef(TemplateDescriptor);
								NewInstanceList.Descriptor = TemplateDescriptor;
								NewInstanceList.PointData = InPointData; // 5.5 only
								NewInstanceList.Instances = Instances;
							}
						}
					}

					if (!CtxHandle.IsValid()) { return; }

					Exit(true);
				},
				LowLevelTasks::ETaskPriority::BackgroundNormal
			);
	}

	return false;
	*/
}

#undef LOCTEXT_NAMESPACE
