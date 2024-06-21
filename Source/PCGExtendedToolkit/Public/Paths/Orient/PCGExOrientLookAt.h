// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOrientOperation.h"
#include "PCGExOrientLookAt.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Orient Look At Mode"))
enum class EPCGExOrientLookAt : uint8
{
	NextPoint UMETA(DisplayName = "Next Point", ToolTip="Look at next point in path"),
	PreviousPoint UMETA(DisplayName = "Previous Point", ToolTip="Look at previous point in path"),
	Direction UMETA(DisplayName = "Direction", ToolTip="Use a local vector attribute as a direction to look at"),
	Position UMETA(DisplayName = "Position", ToolTip="Use a local vector attribtue as a world position to look at"),
};

/**
 * 
 */
UCLASS(DisplayName = "Look At")
class PCGEXTENDEDTOOLKIT_API UPCGExOrientLookAt : public UPCGExOrientOperation
{
	GENERATED_BODY()

public:
	/** Look at method */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOrientLookAt LookAt = EPCGExOrientLookAt::NextPoint;

	/** Vector attribute representing either a direction or world position, depending on selected mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="LookAt==EPCGExOrientLookAt::Direction", EditConditionHides))
	FPCGAttributePropertyInputSelector LookAtAttribute;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual void PrepareForData(PCGExData::FPointIO* InPointIO) override;

	virtual void Orient(PCGEx::FPointRef& Point, const PCGEx::FPointRef& Previous, const PCGEx::FPointRef& Next, const double Factor) const override;

	virtual void LookAtWorldPos(FPCGPoint& Point, const FVector& WorldPos, const double Factor) const;
	virtual void LookAtDirection(FPCGPoint& Point, const int32 Index, const double Factor) const;
	virtual void LookAtPosition(FPCGPoint& Point, const int32 Index, const double Factor) const;

	virtual void Cleanup() override;

protected:
	PCGEx::FLocalVectorGetter* LookAtGetter = nullptr;
};
