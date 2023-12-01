// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGExGraphSolver.h"
#include "PCGExWeightedGraphSolver.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew)
class PCGEXTENDEDTOOLKIT_API UPCGExWeightedGraphSolver : public UPCGExGraphSolver
{
	GENERATED_BODY()

public:
	virtual void InitializeProbe(PCGExGraph::FSocketProbe& Probe) const override;
	virtual bool ProcessPoint(PCGExGraph::FSocketProbe& Probe, const FPCGPoint& Point, const int32 Index) const override;
	virtual void ResolveProbe(PCGExGraph::FSocketProbe& Probe) const override;
};
