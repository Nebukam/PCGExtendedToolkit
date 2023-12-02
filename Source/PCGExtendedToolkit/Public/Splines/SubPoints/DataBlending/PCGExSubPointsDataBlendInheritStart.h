// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsDataBlend.h"
#include "PCGExSubPointsDataBlendInheritStart.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew)
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsDataBlendInheritStart : public UPCGExSubPointsDataBlend
{
	GENERATED_BODY()
public:
	virtual void ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const double PathLength) const override;
};
