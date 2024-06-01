// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCustomGraphSolver.h"
#include "PCGExCustomGraphSolverWeighted.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Weighted Solver", meta=(ToolTip ="Weighted graph solver. Uses Distance-over-Dot curve to find the ideal candidate."))
class PCGEXTENDEDTOOLKIT_API UPCGExCustomGraphSolverWeighted : public UPCGExCustomGraphSolver
{
	GENERATED_BODY()

public:
	FORCEINLINE virtual void InitializeProbe(PCGExGraph::FSocketProbe& Probe) const override;
	FORCEINLINE virtual bool ProcessPoint(PCGExGraph::FSocketProbe& Probe, const PCGEx::FPointRef& Point) const override;
	FORCEINLINE virtual void ResolveProbe(PCGExGraph::FSocketProbe& Probe) const override;
};
