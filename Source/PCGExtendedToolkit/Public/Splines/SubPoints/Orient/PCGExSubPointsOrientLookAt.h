// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsOrientOperation.h"
#include "PCGExSubPointsOrientLookAt.generated.h"

UENUM(BlueprintType)
enum class EPCGExOrientLookAt : uint8
{
	NextPoint UMETA(DisplayName = "Next Point", ToolTip="TBD"),
	PreviousPoint UMETA(DisplayName = "Previous Point", ToolTip="TBD"),
	Attribute UMETA(DisplayName = "Attribute", ToolTip="TBD"),
};

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew, DisplayName = "Look At")
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsOrientLookAt : public UPCGExSubPointsOrientOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExOrientLookAt LookAt = EPCGExOrientLookAt::NextPoint;

	virtual void ProcessSubPoints(const PCGEx::FPointRef& Start, const PCGEx::FPointRef& End, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathInfos& PathInfos) const override;

	virtual void LookAtNext(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const;
	virtual void LookAtPrev(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const;
	virtual void LookAtAttribute(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const;
};
