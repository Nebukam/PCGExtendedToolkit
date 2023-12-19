// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRelaxingOperation.h"
#include "PCGExForceDirectedRelaxing.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DisplayName = "Force-directed")
class PCGEXTENDEDTOOLKIT_API UPCGExForceDirectedRelaxing : public UPCGExEdgeRelaxingOperation
{
	GENERATED_BODY()

public:
	virtual void ProcessVertex(const PCGExMesh::FVertex& Vertex) override;

protected:
};
