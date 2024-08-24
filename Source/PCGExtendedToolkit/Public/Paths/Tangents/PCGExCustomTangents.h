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
	PCGExData::FCache<FVector>* DirectionGetter = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalScale = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseLocalScale"))
	FPCGAttributePropertyInputSelector LocalScale;
	PCGExData::FCache<double>* ScaleGetter = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double DefaultScale = 10;

	void PrepareForData(PCGExData::FFacade* InDataFacade)
	{
		DirectionGetter = InDataFacade->GetBroadcaster<FVector>(Direction);

		if (!bUseLocalScale) { return; }
		ScaleGetter = InDataFacade->GetBroadcaster<double>(LocalScale);
	}

	FORCEINLINE FVector GetDirection(const int32 Index) const
	{
		return DirectionGetter ? DirectionGetter->Values[Index] : FVector::ZeroVector;
	}

	FORCEINLINE FVector GetTangent(const int32 Index) const
	{
		return GetDirection(Index) * (ScaleGetter ? ScaleGetter->Values[Index] : DefaultScale);
	}

	void Cleanup()
	{
		
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

	virtual void PrepareForData(PCGExData::FFacade* InDataFacade) override;
	virtual void ProcessFirstPoint(const TArray<FPCGPoint>& InPoints, FVector& OutArrive, FVector& OutLeave) const override;
	virtual void ProcessLastPoint(const TArray<FPCGPoint>& InPoints, FVector& OutArrive, FVector& OutLeave) const override;
	virtual void ProcessPoint(const TArray<FPCGPoint>& InPoints, const int32 Index, const int32 NextIndex, const int32 PrevIndex, FVector& OutArrive, FVector& OutLeave) const override;

	virtual void Cleanup() override;
};
