// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRelaxingOperation.h"
#include "PCGExForceDirectedRelaxing.generated.h"

/**
 * 
 */
UCLASS(meta=(DisplayName="Force Directed"))
class PCGEXTENDEDTOOLKIT_API UPCGExForceDirectedRelaxing : public UPCGExEdgeRelaxingOperation
{
	GENERATED_BODY()

public:
	virtual void ProcessVertex(const PCGExCluster::FNode& Vertex) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double SpringConstant = 0.1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double ElectrostaticConstant = 1000;

protected:
	void CalculateAttractiveForce(FVector& Force, const FVector& A, const FVector& B) const;
	void CalculateRepulsiveForce(FVector& Force, const FVector& A, const FVector& B) const;
};
