// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "MeshSelectors/PCGMeshSelectorBase.h"
#include "PCGExMacros.h"

#if PCGEX_ENGINE_VERSION > 504
#include "MeshSelectors/PCGISMDescriptor.h"
#endif

#include "PCGExMeshSelectorStaged.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), DisplayName="[PCGEx] Staging Data")
class UPCGExMeshSelectorStaged : public UPCGMeshSelectorBase
{
	GENERATED_BODY()

public:
	virtual bool SelectInstances(
		FPCGStaticMeshSpawnerContext& Context,
		const UPCGStaticMeshSpawnerSettings* Settings,
		const UPCGPointData* InPointData,
		TArray<FPCGMeshInstanceList>& OutMeshInstances,
		UPCGPointData* OutPointData) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MeshSelector, meta = (InlineEditConditionToggle))
	bool bUseAttributeMaterialOverrides = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName = "By Attribute Material Overrides", Category = MeshSelector, meta = (EditCondition = "bUseAttributeMaterialOverrides"))
	TArray<FName> MaterialOverrideAttributes;
};
