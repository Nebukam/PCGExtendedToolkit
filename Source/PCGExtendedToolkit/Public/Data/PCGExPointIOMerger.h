// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "Data/PCGExAttributeHelpers.h"

class PCGEXTENDEDTOOLKIT_API FPCGExPointIOMerger
{
public:
	FPCGExPointIOMerger(PCGExData::FPointIO& OutData);
	virtual ~FPCGExPointIOMerger();

	void Append(PCGExData::FPointIO& InData);
	void Append(const TArray<PCGExData::FPointIO*>& InData);
	void DoMerge();

protected:
	int32 TotalPoints = 0;
	TMap<FName, PCGEx::FAttributeIdentity> Identities;
	TMap<FName, bool> AllowsInterpolation;
	PCGExData::FPointIO* MergedData = nullptr;
	TArray<PCGExData::FPointIO*> MergedPoints;
};
