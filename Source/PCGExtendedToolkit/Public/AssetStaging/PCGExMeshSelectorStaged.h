// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "MeshSelectors/PCGMeshSelectorBase.h"
#include "PCGExMeshSelectorStaged.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), DisplayName="[PCGEx] Staging Data")
class UPCGExMeshSelectorStaged : public UPCGMeshSelectorBase
{
	GENERATED_BODY()

public:
	virtual bool SelectMeshInstances(
		FPCGStaticMeshSpawnerContext& Context,
		const UPCGStaticMeshSpawnerSettings* Settings,
		const UPCGBasePointData* InPointData,
		TArray<FPCGMeshInstanceList>& OutMeshInstances,
		UPCGBasePointData* OutPointData) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MeshSelector, meta = (PCG_Overridable))
	bool bApplyMaterialOverrides = true;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MeshSelector, meta = (PCG_Overridable))
	bool bUseTimeSlicing = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MeshSelector, meta = (PCG_Overridable))
	bool bOutputPoints = true;
};
