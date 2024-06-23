// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGPointData.h"
#include "PCGExPointData.generated.h"


namespace PCGExData
{
	enum class EInit : uint8;
}

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExPointData : public UPCGPointData
{
	GENERATED_BODY()

public:
	virtual void CopyFrom(const UPCGPointData* InPointData);
	virtual void InitializeFromPCGExData(const UPCGExPointData* InPCGExPointData, const PCGExData::EInit InitMode);

	virtual void BeginDestroy() override;
	
protected:
	virtual UPCGSpatialData* CopyInternal() const override;
	
};
