// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExOperation.h"
#include "PCGExTensor.h"
#include "Data/PCGSplineStruct.h"
#include "PCGExTensorOperation.generated.h"

class UPCGExTensorFactoryData;
/**
 * 
 */
UCLASS(Abstract)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTensorOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	TObjectPtr<const UPCGExTensorFactoryData> Factory = nullptr;
	FPCGExTensorConfigBase BaseConfig;

	virtual bool Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory);

	virtual PCGExTensor::FTensorSample SampleAtPosition(const FVector& InPosition) const;
	virtual bool ComputeFactor(const FVector& InPosition, const FPCGPointRef& InEffector, double& OutFactor, FVector& OutGuide) const;
	virtual bool ComputeFactor(const FVector& InPosition, const FPCGSplineStruct& InEffector, const double Radius, FTransform& OutTransform, double& OutFactor, FVector& OutGuide) const;

	virtual void Cleanup() override
	{
		Factory = nullptr;
	}
};

/**
 * 
 */
UCLASS(Abstract)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTensorPointOperation : public UPCGExTensorOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;
	virtual bool Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory) override;

	const UPCGPointData::PointOctree* Octree = nullptr;
	TSharedPtr<PCGExDetails::FDistances> DistanceDetails;
};
