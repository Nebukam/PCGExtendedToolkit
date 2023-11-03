// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGParamData.h"
#include "PCGExRelationalData.generated.h"

class UPCGPointData;
// A sampling direction/cone/volume that can be shared across things
USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSamplingDirection
{
	GENERATED_BODY()

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

// An editable slot, with attribute names and everything
USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExRelationalSlot
{
	GENERATED_BODY()

public:
	/** Name of the attribute to write neighbor index to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName AttributeName = NAME_None;

	/** Slot direction. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ShowOnlyInnerProperties))
	FPCGExSamplingDirection Direction;

	/** Whether to apply point transform to the sampling direction. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bComposeWithTransform = true;
};

// A setting group to be consumed by a PCGExRelationalData
USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExRelationsDefinition
{
	GENERATED_BODY()

public:
	/** List of slot settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(TitleProperty="{AttributeName}"))
	TArray<FPCGExRelationalSlot> Slots = {
		FPCGExRelationalSlot{"Forward", FPCGExSamplingDirection{FVector::ForwardVector}},
		FPCGExRelationalSlot{"Backward", FPCGExSamplingDirection{FVector::BackwardVector}},
		FPCGExRelationalSlot{"Right", FPCGExSamplingDirection{FVector::RightVector}},
		FPCGExRelationalSlot{"Left", FPCGExSamplingDirection{FVector::LeftVector}},
		FPCGExRelationalSlot{"Up", FPCGExSamplingDirection{FVector::UpVector}},
		FPCGExRelationalSlot{"Down", FPCGExSamplingDirection{FVector::DownVector}}
	};
};

// A temp data structure to be used during octree processing
USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FPCGExRelationAttributeData
{
	GENERATED_BODY()

	FPCGExRelationAttributeData()
	{
		Indices.Empty();
	}

public:
	TArray<int64> Indices;

	friend FArchive& operator<<(FArchive& Ar, FPCGExRelationAttributeData& SlotAttData)
	{
		// Implement serialization and deserialization here
		//Ar << SlotAttData.Index;		
		return Ar;
	}

	bool operator==(const FPCGExRelationAttributeData& Other) const
	{
		if (Indices.Num() != Other.Indices.Num()) { return false; }
		for (int i = 0; i < Indices.Num(); i++)
		{
			if (Indices[i] != Other.Indices[i])
			{
				return false;
			}
		}
		return true;
	}

	FPCGExRelationAttributeData operator*(float Weight) const
	{
		FPCGExRelationAttributeData result;
		//TODO: Implement
		return result;
	}

	FPCGExRelationAttributeData operator*(const FPCGExRelationAttributeData Other) const
	{
		FPCGExRelationAttributeData result;
		//TODO: Implement
		return result;
	}

	FPCGExRelationAttributeData operator+(const FPCGExRelationAttributeData Other) const
	{
		FPCGExRelationAttributeData result;
		//TODO: Implement
		return result;
	}

	FPCGExRelationAttributeData operator-(const FPCGExRelationAttributeData Other) const
	{
		FPCGExRelationAttributeData result;
		//TODO: Implement
		return result;
	}

	FPCGExRelationAttributeData operator/(const FPCGExRelationAttributeData Other) const
	{
		FPCGExRelationAttributeData result;
		//TODO: Implement
		return result;
	}

	bool operator<(const FPCGExRelationAttributeData& Other) const
	{
		//TODO: Implement
		return false;
	}
};

// Used to store relational data on a point, as a hidden custom attribute
USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FPCGExRelationCandidateData
{
	GENERATED_BODY()

public:
	int64 Index = -1;
	float MinDistance = TNumericLimits<float>::Max();

	void Reset()
	{
		Index = -1;
		MinDistance = TNumericLimits<float>::Max();
	}
};

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExRelationalData : public UPCGParamData
{
	GENERATED_BODY()

public:
	UPCGExRelationalData(const FObjectInitializer& ObjectInitializer);

	// Checks whether the data have the matching attribute
	bool IsDataReady(UPCGPointData* PointData);
	const TArray<FPCGExRelationalSlot>& GetConstSlots();

	FPCGMetadataAttribute<FPCGExRelationAttributeData>* PrepareData(UPCGPointData* PointData);

public:
	UPROPERTY()
	FName RelationalIdentifier;

	void InitializeLocalDefinition(FPCGExRelationsDefinition Definition);

private:
	UPROPERTY()
	FPCGExRelationsDefinition RelationsDefinition;

	UPROPERTY()
	TArray<FPCGExRelationalSlot> Slots;
};
