// Fill out your copyright notice in the Description page of Project Settings.


#include "Splines/SubPoints/DataBlending/PCGExSubPointsDataBlend.h"

#include "Data/PCGExPointIO.h"


void UPCGExSubPointsDataBlend::PrepareForData(const UPCGExPointIO* InData, PCGEx::FAttributeMap* InAttributeMap)
{
	Super::PrepareForData(InData, InAttributeMap);
}

void UPCGExSubPointsDataBlend::ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const double PathLength) const
{
}
