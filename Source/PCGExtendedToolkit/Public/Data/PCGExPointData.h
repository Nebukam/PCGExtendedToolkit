// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGPointData.h"
#include "PCGExPointData.generated.h"

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExPointData : public UPCGPointData
{
	GENERATED_BODY()

protected:
	virtual UPCGSpatialData* CopyInternal() const override;
	
};
