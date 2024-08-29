// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOrientOperation.h"
#include "PCGExOrientLookAtMode.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Orient Look At Mode"))
enum class EPCGExOrientLookAtMode : uint8
{
	NextPoint UMETA(DisplayName = "Next Point", ToolTip="Look at next point in path"),
	PreviousPoint UMETA(DisplayName = "Previous Point", ToolTip="Look at previous point in path"),
	Direction UMETA(DisplayName = "Direction", ToolTip="Use a local vector attribute as a direction to look at"),
	Position UMETA(DisplayName = "Position", ToolTip="Use a local vector attribtue as a world position to look at"),
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Look At")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExOrientLookAt : public UPCGExOrientOperation
{
	GENERATED_BODY()

public:
	/** Look at method */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOrientLookAtMode LookAt = EPCGExOrientLookAtMode::NextPoint;

	/** Vector attribute representing either a direction or world position, depending on selected mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="LookAt==EPCGExOrientLookAtMode::Direction || LookAt==EPCGExOrientLookAtMode::Position", EditConditionHides))
	FPCGAttributePropertyInputSelector LookAtAttribute;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual bool PrepareForData(PCGExData::FFacade* InDataFacade) override;

	virtual FTransform ComputeOrientation(const PCGExData::FPointRef& Point, const PCGExData::FPointRef& Previous, const PCGExData::FPointRef& Next, const double DirectionMultiplier) const override;

	virtual FTransform LookAtWorldPos(FTransform InT, const FVector& WorldPos, const double DirectionMultiplier) const;
	virtual FTransform LookAtDirection(FTransform InT, const int32 Index, const double DirectionMultiplier) const;
	virtual FTransform LookAtPosition(FTransform InT, const int32 Index, const double DirectionMultiplier) const;

	virtual void Cleanup() override;

protected:
	PCGExData::FCache<FVector>* LookAtGetter = nullptr;
};
