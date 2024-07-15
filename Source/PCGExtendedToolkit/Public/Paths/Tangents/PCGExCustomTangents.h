// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExTangentsOperation.h"
#include "Data/PCGExAttributeHelpers.h"
#include "PCGExCustomTangents.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSingleTangentConfig
{
	GENERATED_BODY()

	FPCGExSingleTangentConfig()
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

	void PrepareForData(const PCGExData::FPointIO* InPointIO)
	{
		DirectionGetter.Capture(Direction);
		DirectionGetter.Grab(InPointIO);
		if (bUseLocalScale)
		{
			ScaleGetter.bEnabled = true;
			ScaleGetter.Capture(LocalScale);
			ScaleGetter.Grab(InPointIO);
		}
		else { ScaleGetter.bEnabled = false; }
	}

	FORCEINLINE FVector GetDirection(const int32 Index) const
	{
		return DirectionGetter[Index];
	}

	FORCEINLINE FVector GetTangent(const int32 Index) const
	{
		return DirectionGetter[Index] * ScaleGetter.SafeGet(Index, DefaultScale);
	}

	void Cleanup()
	{
		DirectionGetter.Cleanup();
		ScaleGetter.Cleanup();
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
	FPCGExSingleTangentConfig Arrive;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bMirror = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="!bMirror"))
	FPCGExSingleTangentConfig Leave;

	virtual void PrepareForData(PCGExData::FPointIO* InPointIO) override;
	virtual void ProcessFirstPoint(const PCGExData::FPointRef& MainPoint, const PCGExData::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const override;
	virtual void ProcessLastPoint(const PCGExData::FPointRef& MainPoint, const PCGExData::FPointRef& PreviousPoint, FVector& OutArrive, FVector& OutLeave) const override;
	virtual void ProcessPoint(const PCGExData::FPointRef& MainPoint, const PCGExData::FPointRef& PreviousPoint, const PCGExData::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const override;

	virtual void Cleanup() override;
};
