// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGEx.h"
#include "PCGExMeshSelectorBase.h"

#include "PCGExMeshSelectorByWeight.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), DisplayName="[PCGEx] Select by Weight")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExMeshSelectorByWeight : public UPCGExMeshSelectorBase
{
	GENERATED_BODY()

public:
	virtual bool Execute(PCGExMeshSelection::FCtx& Ctx) const override;

public:
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MeshSelector, meta=(PCG_Overridable))
	//FName IndexAttributeName;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MeshSelector)
	//EPCGExIndexSafety IndexSafety = EPCGExIndexSafety::Tile;
};
