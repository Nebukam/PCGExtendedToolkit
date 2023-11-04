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

	FPCGExSamplingDirection()
	{
	}

	FPCGExSamplingDirection(const FVector& Dir)
	{
		Direction = Dir;
	}

	FPCGExSamplingDirection(const FPCGExSamplingDirection& Other):
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
	bool bComposeWithPointTransform = true;

	/** Debug color. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FColor DebugColor = FColor::Red;
	
};

USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FPCGExSamplingCandidateData : public FPCGExSamplingDirection
{
	GENERATED_BODY()

	FPCGExSamplingCandidateData()
	{
	}

	FPCGExSamplingCandidateData(const FPCGExSamplingCandidateData& Other):
		FPCGExSamplingDirection(Other),
		Origin(Other.Origin),
		Index(Other.Index),
		IndexedDistance(Other.IndexedDistance),
		IndexedDot(Other.IndexedDot)
	{
	}

	FPCGExSamplingCandidateData(const FPCGExSamplingDirection& Other): FPCGExSamplingDirection(Other)
	{
	}

	FPCGExSamplingCandidateData(const FPCGPoint& Point, const FPCGExRelationalSlot& Slot)
	{
		Direction = Slot.Direction.Direction;
		DotTolerance = Slot.Direction.DotTolerance;
		MaxDistance = Slot.Direction.MaxDistance;

		const FTransform PtTransform = Point.Transform;
		Origin = PtTransform.GetLocation();

		if (Slot.bComposeWithPointTransform)
		{
			Direction = PtTransform.Rotator().RotateVector(Direction);
			Direction.Normalize();
		}
	}

public:
	FVector Origin = FVector::Zero();
	int32 Index = -1;
	double IndexedDistance = TNumericLimits<double>::Max();
	double IndexedDot = -1;

	bool ProcessPoint(const FPCGPoint* Point)
	{
		const FVector PtPosition = Point->Transform.GetLocation();
		const FVector DirToPt = (PtPosition - Origin).GetSafeNormal();

		const double SquaredDistance = FVector::DistSquared(Origin, PtPosition);

		// Is distance smaller than last registered one?
		if (SquaredDistance > IndexedDistance) { return false; }

		// Is distance inside threshold?
		if (SquaredDistance >= (MaxDistance * MaxDistance)) { return false; }

		const double Dot = Direction.Dot(DirToPt);

		// Is dot within tolerance?
		if (Dot < DotTolerance) { return false; }

		if (IndexedDistance == SquaredDistance)
		{
			// In case of distance equality, favor candidate closer to dot == 1
			if (Dot < IndexedDot) { return false; }
		}

		IndexedDistance = SquaredDistance;
		IndexedDot = Dot;

		return true;
	}
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
		FPCGExRelationalSlot{"Forward", FPCGExSamplingDirection{FVector::ForwardVector}, true, FColor(255, 0, 0)},
		FPCGExRelationalSlot{"Backward", FPCGExSamplingDirection{FVector::BackwardVector}, true, FColor(200, 0, 0)},
		FPCGExRelationalSlot{"Right", FPCGExSamplingDirection{FVector::RightVector}, true, FColor(0, 255, 0)},
		FPCGExRelationalSlot{"Left", FPCGExSamplingDirection{FVector::LeftVector}, true, FColor(0, 200, 0)},
		FPCGExRelationalSlot{"Up", FPCGExSamplingDirection{FVector::UpVector}, true, FColor(0, 0, 255)},
		FPCGExRelationalSlot{"Down", FPCGExSamplingDirection{FVector::DownVector}, true, FColor(0, 0, 200)}
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

	FPCGExRelationAttributeData(const TArray<FPCGExSamplingCandidateData>* Candidates)
	{
		Indices.Empty();
		for (const FPCGExSamplingCandidateData& Candidate : *Candidates)
		{
			Indices.Add(Candidate.Index);
		}
	}

public:
	TArray<int64> Indices;
	int64 NumRelations = 0;

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

template <typename T>
concept RelationalDataStruct = std::is_base_of_v<FPCGExRelationAttributeData, T>;

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

public:
	UPROPERTY(BlueprintReadOnly)
	FName RelationalIdentifier;

	UPROPERTY(BlueprintReadOnly)
	FPCGExRelationsDefinition RelationsDefinition;

	UPROPERTY(BlueprintReadOnly)
	TArray<FPCGExRelationalSlot> Slots;

	UPROPERTY(BlueprintReadOnly)
	double GreatestMaxDistance = 0.0;

	void InitializeLocalDefinition(const FPCGExRelationsDefinition& Definition);
};
