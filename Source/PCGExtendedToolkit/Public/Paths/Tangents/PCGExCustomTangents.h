// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExTangentsOperation.h"
#include "Data/PCGExAttributeHelpers.h"
#include "PCGExCustomTangents.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSingleTangentConfig
{
	GENERATED_BODY()

	FPCGExSingleTangentConfig()
	{
		Direction.Update("$Transform.Backward");
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Tangents, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector Direction;
	PCGExData::FCache<FVector>* DirectionGetter = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Tangents, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalScale = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Tangents, meta=(PCG_Overridable, EditCondition="bUseLocalScale"))
	FPCGAttributePropertyInputSelector LocalScale;
	PCGExData::FCache<double>* ScaleGetter = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Tangents, meta=(PCG_Overridable))
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
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName="Custom")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExCustomTangents : public UPCGExTangentsOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Tangents)
	FPCGExSingleTangentConfig Arrive;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Tangents, meta=(InlineEditConditionToggle))
	bool bMirror = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Tangents, meta=(EditCondition="!bMirror"))
	FPCGExSingleTangentConfig Leave;

	virtual void PrepareForData(PCGExData::FFacade* InDataFacade) override
	{
		Super::PrepareForData(InDataFacade);
		Arrive.PrepareForData(InDataFacade);
		Leave.PrepareForData(InDataFacade);
	}

	FORCEINLINE virtual void ProcessFirstPoint(const TArray<FPCGPoint>& InPoints, FVector& OutArrive, FVector& OutLeave) const override
	{
		OutArrive = Arrive.GetTangent(0);
		OutLeave = bMirror ? Arrive.GetTangent(0) : Leave.GetTangent(0);
	}

	FORCEINLINE virtual void ProcessLastPoint(const TArray<FPCGPoint>& InPoints, FVector& OutArrive, FVector& OutLeave) const override
	{
		const int32 Index = InPoints.Num() - 1;
		OutArrive = Arrive.GetTangent(Index);
		OutLeave = bMirror ? Arrive.GetTangent(Index) : Leave.GetTangent(Index);
	}

	FORCEINLINE virtual void ProcessPoint(const TArray<FPCGPoint>& InPoints, const int32 Index, const int32 NextIndex, const int32 PrevIndex, FVector& OutArrive, FVector& OutLeave) const override
	{
		OutArrive = Arrive.GetTangent(Index);
		OutLeave = bMirror ? Arrive.GetTangent(Index) : Leave.GetTangent(Index);
	}
};
