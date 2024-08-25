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
#include "Metadata/PCGObjectPropertyOverride.h"
#include "Metadata/PCGMetadataPartitionCommon.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "Engine/StaticMesh.h"
#include "MeshSelectors/PCGExMeshCollection.h"

#define LOCTEXT_NAMESPACE "PCGMeshSelectorBase"

namespace PCGMeshSelectorBase
{
	// Returns variation based on mesh, material overrides and reverse culling
	FPCGMeshInstanceList& GetInstanceList(
		TArray<FPCGMeshInstanceList>& InstanceLists,
		const FSoftISMComponentDescriptor& TemplateDescriptor,
		TSoftObjectPtr<UStaticMesh> Mesh,
		const TArray<TSoftObjectPtr<UMaterialInterface>>& MaterialOverrides,
		bool bReverseCulling,
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

		return NewInstanceList;
	}
}

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

	if (!Execute(
		Context,
		Settings,
		InPointData,
		OutMeshInstances,
		OutPointData,
		OutPoints,
		OutAttribute))
	{
		return false;
	}

	if (Context.CurrentPointIndex == InPointData->GetPoints().Num())
	{
		TArray<TArray<FPCGMeshInstanceList>>& MeshInstances = Context.WeightedMeshInstances;
		CollapseInstances(MeshInstances, OutMeshInstances);
		return true;
	}

	return false;
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

bool UPCGExMeshSelectorBase::Execute(
	FPCGStaticMeshSpawnerContext& Context,
	const UPCGStaticMeshSpawnerSettings* Settings,
	const UPCGPointData* InPointData,
	TArray<FPCGMeshInstanceList>& OutMeshInstances,
	UPCGPointData* OutPointData,
	TArray<FPCGPoint>* OutPoints,
	FPCGMetadataAttribute<FString>* OutAttributeId) const
{
	Context.CurrentPointIndex = InPointData->GetPoints().Num() - 1;
	return true;
}

void UPCGExMeshSelectorBase::CollapseInstances(TArray<TArray<FPCGMeshInstanceList>>& MeshInstances, TArray<FPCGMeshInstanceList>& OutMeshInstances) const
{
	for (TArray<FPCGMeshInstanceList>& PickedMeshInstances : MeshInstances)
	{
		for (FPCGMeshInstanceList& PickedMeshInstanceEntry : PickedMeshInstances)
		{
			OutMeshInstances.Emplace(MoveTemp(PickedMeshInstanceEntry));
		}
	}
}

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
			if (Pick.Matches(InstanceList) && InstanceList.AttributePartitionIndex == AttributePartitionIndex)
			{
				return InstanceList;
			}
		}

		FPCGMeshInstanceList& NewInstanceList = InstanceLists.Emplace_GetRef(Pick.Descriptor);
		NewInstanceList.Descriptor.StaticMesh = Pick.Descriptor.StaticMesh;
		//NewInstanceList.Descriptor.OverrideMaterials = MaterialOverrides;
		NewInstanceList.Descriptor.bReverseCulling = bReverseCulling;
		NewInstanceList.AttributePartitionIndex = AttributePartitionIndex;

		return NewInstanceList;
	}
}

#undef LOCTEXT_NAMESPACE
