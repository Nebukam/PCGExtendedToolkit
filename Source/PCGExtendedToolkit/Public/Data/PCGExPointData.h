// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMacros.h"
#include "Data/PCGPointData.h"
#include "PCGExPointData.generated.h"


namespace PCGExData
{
	enum class EIOInit : uint8;
}

/**
 * 
 */
UCLASS(Abstract)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPointData : public UPCGPointData
{
	GENERATED_BODY()

public:
	virtual void CopyFrom(const UPCGPointData* InPointData);
	virtual void InitializeFromPCGExData(const UPCGExPointData* InPCGExPointData, const PCGExData::EIOInit InitMode);

	virtual void BeginDestroy() override;

protected:
#if PCGEX_ENGINE_VERSION < 505
	virtual UPCGSpatialData* CopyInternal() const override;
#else
	virtual UPCGSpatialData* CopyInternal(FPCGContext* Context) const override;
#endif
};
