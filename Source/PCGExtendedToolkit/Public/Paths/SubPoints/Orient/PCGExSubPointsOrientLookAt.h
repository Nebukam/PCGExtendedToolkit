// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsOrientOperation.h"
#include "PCGExSubPointsOrientLookAt.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Orient Look At Mode"))
enum class EPCGExOrientLookAt : uint8
{
	NextPoint UMETA(DisplayName = "Next Point", ToolTip="Look at next point in path"),
	PreviousPoint UMETA(DisplayName = "Previous Point", ToolTip="Look at previous point in path"),
	Attribute UMETA(DisplayName = "Attribute", ToolTip="Look at a local vector attribute representing a world position"),
};

/**
 * 
 */
UCLASS(DisplayName = "Look At")
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsOrientLookAt : public UPCGExSubPointsOrientOperation
{
	GENERATED_BODY()

public:
	/** Look at method */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOrientLookAt LookAt = EPCGExOrientLookAt::NextPoint;

	/** Vector attribute representing a world position */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="LookAt==EPCGExOrientLookAt::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector LookAtSelector;

	/** Use attribute value as a world space offset from point position */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="LookAt==EPCGExOrientLookAt::Attribute", EditConditionHides))
	bool bAttributeAsOffset = false;

	virtual void PrepareForData(PCGExData::FPointIO& InPointIO) override;

	virtual void ProcessSubPoints(const PCGEx::FPointRef& Start, const PCGEx::FPointRef& End, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetricsSquared& Metrics) const override;

	virtual void LookAtNext(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const;
	virtual void LookAtPrev(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const;
	virtual void LookAtAttribute(FPCGPoint& Point, const int32 Index) const;
	virtual void LookAtAttributeOffset(FPCGPoint& Point, const int32 Index) const;

	virtual void Cleanup() override;

protected:
	PCGEx::FLocalVectorGetter* LookAtGetter = nullptr;
	virtual void ApplyOverrides() override;
};
