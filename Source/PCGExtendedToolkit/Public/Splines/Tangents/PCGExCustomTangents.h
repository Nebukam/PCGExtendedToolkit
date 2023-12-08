// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExTangentsOperation.h"
#include "Data/PCGExAttributeHelpers.h"
#include "PCGExCustomTangents.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSingleTangentParams
{
	GENERATED_BODY()

	FPCGExSingleTangentParams()
	{
		Direction.Selector.Update("$Transform");
		Direction.Axis = EPCGExAxis::Backward;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExInputDescriptorWithDirection Direction;
	PCGEx::FLocalDirectionGetter DirectionGetter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bUseLocalScale = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bUseLocalScale"))
	FPCGExInputDescriptorWithSingleField LocalScale;
	PCGEx::FLocalSingleFieldGetter ScaleGetter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	double DefaultScale = 10;

	void PrepareForData(const UPCGPointData* InData)
	{
		DirectionGetter.Capture(Direction);
		DirectionGetter.Validate(InData);

		if (bUseLocalScale)
		{
			ScaleGetter.bEnabled = true;
			ScaleGetter.Capture(LocalScale);
			ScaleGetter.Validate(InData);
		}
		else { ScaleGetter.bEnabled = false; }
	}

	FVector GetDirection(const FPCGPoint& Point) const { return DirectionGetter.GetValue(Point); }
	FVector GetTangent(const FPCGPoint& Point) const { return DirectionGetter.GetValue(Point) * ScaleGetter.GetValueSafe(Point, DefaultScale); }
};

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew)
class PCGEXTENDEDTOOLKIT_API UPCGExCustomTangents : public UPCGExTangentsOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExSingleTangentParams Arrive;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bMirror = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="!bMirror"))
	FPCGExSingleTangentParams Leave;

	virtual void PrepareForData(PCGExData::FPointIO* InPath) override;
	virtual void ProcessFirstPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& NextPoint) const override;
	virtual void ProcessLastPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& PreviousPoint) const override;
	virtual void ProcessPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const override;
};
