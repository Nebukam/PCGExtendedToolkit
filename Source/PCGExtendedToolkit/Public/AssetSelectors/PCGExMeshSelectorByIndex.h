// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGEx.h"
#include "PCGExAssetCollection.h"
#include "PCGExMeshSelectorBase.h"

#include "PCGExMeshSelectorByIndex.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), DisplayName="[PCGEx] Select by Index")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExMeshSelectorByIndex : public UPCGExMeshSelectorBase
{
	GENERATED_BODY()

public:
	virtual bool Setup(FPCGStaticMeshSpawnerContext& Context, const UPCGStaticMeshSpawnerSettings* Settings, const UPCGPointData* InPointData, UPCGPointData* OutPointData) const override;
	virtual bool Execute(PCGExMeshSelection::FCtx& Ctx) const override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MeshSelector, meta=(PCG_Overridable))
	FName IndexAttribute = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MeshSelector)
	EPCGExIndexPickMode PickMode = EPCGExIndexPickMode::Ascending;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MeshSelector)
	EPCGExIndexSafety IndexSafety = EPCGExIndexSafety::Tile;
};
