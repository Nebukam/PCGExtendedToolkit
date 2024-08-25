// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "MeshSelectors/PCGExMeshSelectorBase.h"

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

	// TODO: Remove if/when FBodyInstance is updated or replaced
	// Necessary to update the collision Response Container from the Response Array
	// TemplateDescriptor.PostLoadFixup(this);
}

bool UPCGExMeshSelectorBase::SelectInstances(
	FPCGStaticMeshSpawnerContext& Context,
	const UPCGStaticMeshSpawnerSettings* Settings,
	const UPCGPointData* InPointData,
	TArray<FPCGMeshInstanceList>& OutMeshInstances,
	UPCGPointData* OutPointData) const
{
	return false;
}

#undef LOCTEXT_NAMESPACE
