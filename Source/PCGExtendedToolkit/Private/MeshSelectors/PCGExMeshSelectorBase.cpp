// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "MeshSelectors/PCGExMeshSelectorBase.h"

#include "PCGExMacros.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Elements/PCGStaticMeshSpawnerContext.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
#include "Metadata/PCGObjectPropertyOverride.h"
#include "Metadata/PCGMetadataPartitionCommon.h"
#endif

#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "Engine/StaticMesh.h"
#include "MeshSelectors/PCGExMeshCollection.h"

#define LOCTEXT_NAMESPACE "PCGMeshSelectorBase"

void UPCGExMeshSelectorBase::PostLoad()
{
	Super::PostLoad();
	RefreshInternal();
}

#if WITH_EDITOR
void UPCGExMeshSelectorBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshInternal();
}
#endif

bool UPCGExMeshSelectorBase::SelectInstances(
	FPCGStaticMeshSpawnerContext& Context,
	const UPCGStaticMeshSpawnerSettings* Settings,
	const UPCGPointData* InPointData,
	TArray<FPCGMeshInstanceList>& OutMeshInstances,
	UPCGPointData* OutPointData) const
{
	if (!InPointData)
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, FTEXT("Missing input data"));
		return true;
	}

	if (Context.CurrentPointIndex == 0) { if (!Setup(Context, Settings, InPointData, OutPointData)) { return true; } }

	TArray<FPCGPoint>* OutPoints = nullptr;
	FPCGMetadataAttribute<FString>* OutAttribute = nullptr;

	if (OutPointData)
	{
		OutAttribute = OutPointData->Metadata->GetMutableTypedAttribute<FString>(Settings->OutAttributeName);
		OutPoints = OutAttribute ? &OutPointData->GetMutablePoints() : nullptr;
	}

	PCGExMeshSelection::FCtx Data = PCGExMeshSelection::FCtx(
		&Context, Settings, InPointData, &OutMeshInstances,
		OutPointData, OutPoints, OutAttribute);

	if (!Execute(Data)) { return false; }

	return Context.CurrentPointIndex == InPointData->GetPoints().Num();
}

void UPCGExMeshSelectorBase::BeginDestroy()
{
	MainCollectionPtr = nullptr;
	Super::BeginDestroy();
}

void UPCGExMeshSelectorBase::RefreshInternal()
{
	if (MainCollection.ToSoftObjectPath().IsValid())
	{
		MainCollectionPtr = MainCollection.LoadSynchronous();
		if (MainCollectionPtr) { MainCollectionPtr->RebuildCachedData(); }
	}
}

bool UPCGExMeshSelectorBase::Setup(
	FPCGStaticMeshSpawnerContext& Context,
	const UPCGStaticMeshSpawnerSettings* Settings,
	const UPCGPointData* InPointData,
	UPCGPointData* OutPointData) const
{
	if (OutPointData)
	{
		check(OutPointData->Metadata);

		if (!OutPointData->Metadata->HasAttribute(Settings->OutAttributeName))
		{
			PCGE_LOG_C(Error, GraphAndLog, &Context, FText::Format(LOCTEXT("AttributeNotInMetadata", "Out attribute '{0}' is not in the metadata"), FText::FromName(Settings->OutAttributeName)));
			return false;
		}

		if (FPCGMetadataAttributeBase* OutAttributeBase = OutPointData->Metadata->GetMutableAttribute(Settings->OutAttributeName))
		{
			if (OutAttributeBase->GetTypeId() != PCG::Private::MetadataTypes<FString>::Id)
			{
				PCGE_LOG_C(Error, GraphAndLog, &Context, LOCTEXT("AttributeNotFString", "Out attribute is not of valid type FString"));
				return false;
			}
		}
	}

	if (!MainCollectionPtr)
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, FTEXT("Missing collection data"));
		return false;
	}

	/*
	// TODO : Expose when API is available
	FPCGMeshMaterialOverrideHelper& MaterialOverrideHelper = Context.MaterialOverrideHelper;
	if (!MaterialOverrideHelper.IsInitialized())
	{
		MaterialOverrideHelper.Initialize(Context, bUseAttributeMaterialOverrides, MaterialOverrideAttributes, InPointData->Metadata);
	}

	if (!MaterialOverrideHelper.IsValid()) { return false; }
	*/

	return true;
}

bool UPCGExMeshSelectorBase::Execute(PCGExMeshSelection::FCtx& Ctx) const
{
	Ctx.Context->CurrentPointIndex = Ctx.InPointData->GetPoints().Num() - 1;
	return true;
}

FPCGMeshInstanceList& UPCGExMeshSelectorBase::RegisterPick(
	const FPCGExMeshCollectionEntry& Entry,
	const FPCGPoint& Point, const int32 PointIndex,
	PCGExMeshSelection::FCtx& Ctx) const
{
	const bool bNeedsReverseCulling = (Point.Transform.GetDeterminant() < 0);
	FPCGMeshInstanceList& InstanceList = GetInstanceList(*Ctx.OutMeshInstances, Entry, bNeedsReverseCulling);

	InstanceList.Instances.Emplace(Point.Transform);
	InstanceList.InstancesMetadataEntry.Emplace(Point.MetadataEntry);

	if (Ctx.OutPoints && Ctx.OutAttributeId)
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <= 3
		TMap<TSoftObjectPtr<UStaticMesh>, FBox>& MeshToBoundingBox = Ctx.Context->MeshToBoundingBox;
#endif

		const TSoftObjectPtr<UStaticMesh>& Mesh = InstanceList.Descriptor.StaticMesh;

		FPCGPoint& OutPoint = Ctx.OutPoints->Add_GetRef(Point);
		PCGMetadataValueKey* OutValueKey = Ctx.Context->MeshToValueKey.Find(Mesh);
		if (!OutValueKey)
		{
			PCGMetadataValueKey ValueKey = Ctx.OutAttributeId->AddValue(Mesh.ToSoftObjectPath().ToString());
			OutValueKey = &Ctx.Context->MeshToValueKey.Add(Mesh, ValueKey);
		}

		check(OutValueKey);

		Ctx.OutPointData->Metadata->InitializeOnSet(OutPoint.MetadataEntry);
		Ctx.OutAttributeId->SetValueFromValueKey(OutPoint.MetadataEntry, *OutValueKey);

		if (Ctx.Settings->bApplyMeshBoundsToPoints)
		{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3

			TArray<int32>& PointIndices = Ctx.Context->MeshToOutPoints.FindOrAdd(Mesh).FindOrAdd(Ctx.OutPointData);
			PointIndices.Emplace(PointIndex);

#else

			if (!MeshToBoundingBox.Contains(Mesh) && Mesh.LoadSynchronous())
			{
				MeshToBoundingBox.Add(Mesh, Mesh->GetBoundingBox());
			}

			if (MeshToBoundingBox.Contains(Mesh))
			{
				const FBox MeshBounds = MeshToBoundingBox[Mesh];
				OutPoint.BoundsMin = MeshBounds.Min;
				OutPoint.BoundsMax = MeshBounds.Max;
			}
			
#endif
		}
	}

	return InstanceList;
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3

FPCGMeshInstanceList& UPCGExMeshSelectorBase::GetInstanceList(
	TArray<FPCGMeshInstanceList>& InstanceLists,
	const FPCGExMeshCollectionEntry& Pick,
	bool bReverseCulling,
	const int AttributePartitionIndex) const
{
	{
		// Find instance list that matches the provided pick
		// TODO : Account for material overrides

		for (FPCGMeshInstanceList& InstanceList : InstanceLists)
		{
			if (Pick.Matches(InstanceList) && InstanceList.AttributePartitionIndex == AttributePartitionIndex) { return InstanceList; }
		}

		FPCGMeshInstanceList& NewInstanceList = InstanceLists.Emplace_GetRef(Pick.Descriptor);
		NewInstanceList.Descriptor.StaticMesh = Pick.Descriptor.StaticMesh;
		//NewInstanceList.Descriptor.OverrideMaterials = MaterialOverrides;
		NewInstanceList.Descriptor.bReverseCulling = bReverseCulling;
		NewInstanceList.AttributePartitionIndex = AttributePartitionIndex;

		return NewInstanceList;
	}
}

#else

FPCGMeshInstanceList& UPCGExMeshSelectorBase::GetInstanceList(
	TArray<FPCGMeshInstanceList>& InstanceLists,
	const FPCGExMeshCollectionEntry& Pick,
	bool bReverseCulling) const
{
	{
		// Find instance list that matches the provided pick
		// TODO : Account for material overrides

		for (FPCGMeshInstanceList& InstanceList : InstanceLists)
		{
			if (Pick.Matches(InstanceList)) { return InstanceList; }
		}

		FPCGMeshInstanceList& NewInstanceList = InstanceLists.Emplace_GetRef(Pick.Descriptor);
		NewInstanceList.Descriptor.StaticMesh = Pick.Descriptor.StaticMesh;
		//NewInstanceList.Descriptor.OverrideMaterials = MaterialOverrides;
		NewInstanceList.Descriptor.bReverseCulling = bReverseCulling;

		return NewInstanceList;
	}
}
#endif

#undef LOCTEXT_NAMESPACE
