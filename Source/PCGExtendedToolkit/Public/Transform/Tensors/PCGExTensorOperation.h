// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExOperation.h"
#include "PCGExTensor.h"
#include "Data/PCGSplineStruct.h"
#include "Paths/PCGExPaths.h"
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

	virtual PCGExTensor::FTensorSample Sample(const FTransform& InProbe) const;

	template <bool bFast = false>
	bool ComputeFactor(const FVector& InPosition, const FPCGPointRef& InEffector, PCGExTensor::FEffectorMetrics& OutMetrics) const
	{
		const FVector Center = InEffector.Point->Transform.GetLocation();
		const double RadiusSquared = InEffector.Point->Color.W;
		const double DistSquared = FVector::DistSquared(InPosition, Center);

		if (FVector::DistSquared(InPosition, Center) > RadiusSquared) { return false; }

		const double OutFactor = DistSquared / RadiusSquared;
		OutMetrics.Factor = OutFactor;

		if constexpr (bFast) { OutMetrics.Guide = FVector::ForwardVector; }
		else { OutMetrics.Guide = BaseConfig.LocalGuideCurve.GetValue(OutFactor); }

		OutMetrics.Potency = InEffector.Point->Steepness * BaseConfig.PotencyFalloffCurveObj->Eval(OutFactor);
		OutMetrics.Weight = InEffector.Point->Density * BaseConfig.WeightFalloffCurveObj->Eval(OutFactor);

		return true;
	}

	template <bool bFast = false>
	bool ComputeFactor(const FVector& InPosition, const FPCGSplineStruct& InEffector, const double Radius, FTransform& OutTransform, PCGExTensor::FEffectorMetrics& OutMetrics) const
	{
		OutTransform = PCGExPaths::GetClosestTransform(InEffector, InPosition, true);

		const FVector Scale = OutTransform.GetScale3D();

		const double RadiusSquared = FMath::Square(FVector2D(Scale.Y, Scale.Z).Length() * Radius);
		const double DistSquared = FVector::DistSquared(InPosition, OutTransform.GetLocation());

		if (DistSquared > RadiusSquared) { return false; }

		const double OutFactor = DistSquared / RadiusSquared;
		OutMetrics.Factor = OutFactor;

		if constexpr (bFast) { OutMetrics.Guide = FVector::ForwardVector; }
		else { OutMetrics.Guide = BaseConfig.LocalGuideCurve.GetValue(OutFactor); }

		OutMetrics.Potency = BaseConfig.Potency * BaseConfig.PotencyFalloffCurveObj->Eval(OutFactor);
		OutMetrics.Weight = BaseConfig.Weight * BaseConfig.WeightFalloffCurveObj->Eval(OutFactor);

		return true;
	}

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
