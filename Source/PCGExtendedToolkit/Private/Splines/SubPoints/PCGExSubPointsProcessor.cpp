// Fill out your copyright notice in the Description page of Project Settings.


#include "Splines/SubPoints/PCGExSubPointsProcessor.h"

#include "Data/PCGExPointIO.h"


void UPCGExSubPointsProcessor::PrepareForData(const UPCGExPointIO* InData, PCGEx::FAttributeMap* InAttributeMap)
{
	AttributeMap = InAttributeMap;
}

void UPCGExSubPointsProcessor::ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const double PathLength) const
{
	
}
