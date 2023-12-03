// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGPointData.h"
#include "UObject/Object.h"
#include "PCGExMetadataProxy.generated.h"

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExMetadataProxy : public UObject
{
	GENERATED_BODY()

public:
	void PrepareForData(UPCGPointData* InData);

protected:
	
};
