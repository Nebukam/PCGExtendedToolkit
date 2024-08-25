// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGEx.h"
#include "PCGExMeshSelectorBase.h"

#include "PCGExMeshSelectorByIndex.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExMeshSelectorByIndex : public UPCGExMeshSelectorBase
{
	GENERATED_BODY()

public:
	virtual bool Execute(
		FPCGStaticMeshSpawnerContext& Context,
		const UPCGStaticMeshSpawnerSettings* Settings,
		const UPCGPointData* InPointData,
		TArray<FPCGMeshInstanceList>& OutMeshInstances,
		UPCGPointData* OutPointData,
		TArray<FPCGPoint>* OutPoints,
		FPCGMetadataAttribute<FString>* OutAttributeId) const override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MeshSelector, meta=(PCG_Overridable))
	FName IndexAttributeName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MeshSelector)
	EPCGExIndexSafety IndexSafety = EPCGExIndexSafety::Tile;
};
