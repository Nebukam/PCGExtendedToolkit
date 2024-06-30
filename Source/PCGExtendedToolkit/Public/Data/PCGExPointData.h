// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

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
