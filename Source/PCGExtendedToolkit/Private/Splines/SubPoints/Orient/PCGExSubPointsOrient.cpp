// Fill out your copyright notice in the Description page of Project Settings.


#include "Splines/SubPoints/Orient/PCGExSubPointsOrient.h"

#include "Data/PCGExPointIO.h"


void UPCGExSubPointsOrient::PrepareForData(const UPCGExPointIO* InData, PCGEx::FAttributeMap* InAttributeMap)
{
	Super::PrepareForData(InData, InAttributeMap);
}

void UPCGExSubPointsOrient::ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const double PathLength) const
{
}
