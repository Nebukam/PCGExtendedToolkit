// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGParamData.h"
#include "PCGExRelationalParamsData.generated.h"

class UPCGPointData;

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSamplingModifier : public FPCGExSelectorSettingsBase
{
	GENERATED_BODY()

	FPCGExSamplingModifier(): FPCGExSelectorSettingsBase()
	{
	}

	FPCGExSamplingModifier(const FPCGExSamplingModifier& Other): FPCGExSelectorSettingsBase(Other)
	{
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExRelationDirection
{
	GENERATED_BODY()

	FPCGExRelationDirection()
	{
	}

	FPCGExRelationDirection(const FVector& Dir)
	{
		Direction = Dir;
	}

	FPCGExRelationDirection(const FPCGExRelationDirection& Other):
		Direction(Other.Direction),
		DotTolerance(Other.DotTolerance),
		MaxDistance(Other.MaxDistance)
	{
	}

public:
	/** Slot 'look-at' direction. Used along with DotTolerance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector Direction = FVector::UpVector;

	/** Tolerance threshold. Used along with the direction of the slot when looking for the closest candidate. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	double DotTolerance = 0.707f; // 45deg

	/** Maximum sampling distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	double MaxDistance = 1000.0f;
};

// An editable relation slot
USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExRelationDefinition
{
	GENERATED_BODY()

public:
	/** Name of the attribute to write neighbor index to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName AttributeName = NAME_None;

	/** Relation direction in space. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ShowOnlyInnerProperties))
	FPCGExRelationDirection Direction;

	/** Whether the orientation of the direction is relative to the point or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bRelativeOrientation = true;

	/** Is the distance modified by local attributes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bApplyAttributeModifier = false;

	/** Which local attribute is used to factor the distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bApplyAttributeModifier", EditConditionHides))
	FPCGExSamplingModifier AttributeModifier;

	/** Whether this slot is enabled or not. Handy to do trial-and-error without adding/deleting array elements. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(AdvancedDisplay))
	bool bEnabled = true;

	/** Debug color. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(AdvancedDisplay))
	FColor DebugColor = FColor::Red;
};

// A setting group to be consumed by a PCGExRelationalParamsData
USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExRelationsDefinition
{
	GENERATED_BODY()

public:
	/** List of slot settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(TitleProperty="{AttributeName}"))
	TArray<FPCGExRelationDefinition> RelationSlots = {
		FPCGExRelationDefinition{"Forward", FPCGExRelationDirection{FVector::ForwardVector}, true, false, FPCGExSamplingModifier{}, true, FColor(255, 0, 0)},
		FPCGExRelationDefinition{"Backward", FPCGExRelationDirection{FVector::BackwardVector}, true, false, FPCGExSamplingModifier{}, true, FColor(200, 0, 0)},
		FPCGExRelationDefinition{"Right", FPCGExRelationDirection{FVector::RightVector}, true, false, FPCGExSamplingModifier{}, true, FColor(0, 255, 0)},
		FPCGExRelationDefinition{"Left", FPCGExRelationDirection{FVector::LeftVector}, true, false, FPCGExSamplingModifier{}, true, FColor(0, 200, 0)},
		FPCGExRelationDefinition{"Up", FPCGExRelationDirection{FVector::UpVector}, true, false, FPCGExSamplingModifier{}, true, FColor(0, 0, 255)},
		FPCGExRelationDefinition{"Down", FPCGExRelationDirection{FVector::DownVector}, true, false, FPCGExSamplingModifier{}, true, FColor(0, 0, 200)}
	};
};

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExRelationalParamsData : public UPCGPointData
{
	GENERATED_BODY()

public:
	UPCGExRelationalParamsData(const FObjectInitializer& ObjectInitializer);

	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Param; }

	// Checks whether the data have the matching attribute
	bool IsDataReady(UPCGPointData* PointData);
	const TArray<FPCGExRelationDefinition>& GetConstSlots();

public:
	UPROPERTY(BlueprintReadOnly)
	FName RelationalIdentifier;

	UPROPERTY(BlueprintReadOnly)
	FPCGExRelationsDefinition RelationsDefinition;

	UPROPERTY(BlueprintReadOnly)
	TArray<FPCGExRelationDefinition> RelationSlots;

	UPROPERTY(BlueprintReadOnly)
	double GreatestStaticMaxDistance = 0.0;

	UPROPERTY(BlueprintReadOnly)
	bool bMarkMutualRelations = true;

	UPROPERTY(BlueprintReadOnly)
	bool bHasVariableMaxDistance = false;

public:
	void Initialize(FName Identifier, const FPCGExRelationsDefinition& Definition);
	
	bool PrepareSelectors(const UPCGPointData* PointData, TArray<FPCGExSamplingModifier>& OutSelectors) const;

};
