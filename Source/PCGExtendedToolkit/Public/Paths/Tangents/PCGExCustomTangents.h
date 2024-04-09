// Copyright Timothé Lapetite 2024
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
		Direction.Update("$Transform.Backward");
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector Direction;
	PCGEx::FLocalVectorGetter DirectionGetter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalScale = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseLocalScale"))
	FPCGAttributePropertyInputSelector LocalScale;
	PCGEx::FLocalSingleFieldGetter ScaleGetter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double DefaultScale = 10;

	void PrepareForData(const PCGExData::FPointIO& InData)
	{
		DirectionGetter.Capture(Direction);
		DirectionGetter.Grab(InData);
		if (bUseLocalScale)
		{
			ScaleGetter.bEnabled = true;
			ScaleGetter.Capture(LocalScale);
			ScaleGetter.Grab(InData);
		}
		else { ScaleGetter.bEnabled = false; }
	}

	FVector GetDirection(const int32 Index) const { return DirectionGetter[Index]; }
	FVector GetTangent(const int32 Index) const { return DirectionGetter[Index] * ScaleGetter.SafeGet(Index, DefaultScale); }

	void Cleanup()
	{
		PCGEX_CLEANUP(DirectionGetter)
		PCGEX_CLEANUP(ScaleGetter)
	}
};

/**
 * 
 */
UCLASS(DisplayName="Custom")
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

	virtual void PrepareForData(PCGExData::FPointIO& InPointIO) override;
	virtual void ProcessFirstPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const override;
	virtual void ProcessLastPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& PreviousPoint, FVector& OutArrive, FVector& OutLeave) const override;
	virtual void ProcessPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& PreviousPoint, const PCGEx::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const override;

	virtual void Cleanup() override;
};
