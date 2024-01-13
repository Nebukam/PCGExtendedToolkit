// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCustomGraphSolver.h"
#include "PCGExCustomGraphSolverWeighted.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Weighted Solver")
class PCGEXTENDEDTOOLKIT_API UPCGExCustomGraphSolverWeighted : public UPCGExCustomGraphSolver
{
	GENERATED_BODY()

public:
	virtual void InitializeProbe(PCGExGraph::FSocketProbe& Probe) const override;
	virtual bool ProcessPoint(PCGExGraph::FSocketProbe& Probe, const PCGEx::FPointRef& Point) const override;
	virtual void ResolveProbe(PCGExGraph::FSocketProbe& Probe) const override;
};
