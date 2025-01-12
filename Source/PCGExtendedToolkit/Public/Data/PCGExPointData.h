// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMacros.h"
#include "Data/PCGPointData.h"
#include "PCGExPointData.generated.h"

#if PCGEX_ENGINE_VERSION < 505
#define PCGEX_DATA_COPY_INTERNAL_DECL virtual UPCGSpatialData* CopyInternal() const override;
#define PCGEX_DATA_COPY_INTERNAL_IMPL(_TYPE) UPCGSpatialData* _TYPE::CopyInternal() const{\
	_TYPE* NewPointData = nullptr; { FGCScopeGuard GCGuard; NewPointData = Cast<_TYPE>(NewObject<UObject>(GetTransientPackage(), GetClass())); } NewPointData->CopyFrom(this);	return NewPointData; }
#else
#define PCGEX_DATA_COPY_INTERNAL_DECL virtual UPCGSpatialData* CopyInternal(FPCGContext* Context) const override;
#define PCGEX_DATA_COPY_INTERNAL_IMPL(_TYPE)UPCGSpatialData* _TYPE::CopyInternal(FPCGContext* Context) const{\
	_TYPE* NewPointData = Cast<_TYPE>(FPCGContext::NewObject_AnyThread<UObject>(Context, GetTransientPackage(), GetClass())); NewPointData->CopyFrom(this);return NewPointData;}
#endif

//#define PCGEX_DATA_COPY_INTERNAL_IMPL(_TYPE)UPCGSpatialData* _TYPE::CopyInternal(FPCGContext* Context) const{\
//_TYPE* NewPointData = FPCGContext::NewObject_AnyThread<_TYPE>(Context);	NewPointData->CopyFrom(this);return NewPointData;}
namespace PCGExData
{
	enum class EIOInit : uint8;
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
	virtual void InitializeFromPCGExData(const UPCGExPointData* InPCGExPointData, const PCGExData::EIOInit InitMode);

	virtual void BeginDestroy() override;

protected:
	PCGEX_DATA_COPY_INTERNAL_DECL
};
