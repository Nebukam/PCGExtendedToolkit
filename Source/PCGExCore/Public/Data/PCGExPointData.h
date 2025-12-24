// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGPointArrayData.h"
#include "PCGExPointData.generated.h"

#define PCGEX_NEW_CUSTOM_POINT_DATA(_TYPE)\
	_TYPE* NewData = Cast<_TYPE>(FPCGContext::NewObject_AnyThread<_TYPE>(Context)); \
	if (!SupportsSpatialDataInheritance()){ NewData->CopyUnallocatedPropertiesFrom(this); }

namespace PCGExData
{
	enum class EIOInit : uint8;
}

/**
 * 
 */
UCLASS(Abstract)
class PCGEXCORE_API UPCGExPointData : public UPCGPointArrayData
{
	GENERATED_BODY()

protected:
	virtual UPCGSpatialData* CopyInternal(FPCGContext* Context) const override;
};
