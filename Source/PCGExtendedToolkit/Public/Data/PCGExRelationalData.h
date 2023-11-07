// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGElement.h"
#include "PCGExCommon.h"
#include "PCGParamData.h"
#include "Data/PCGExRelationalParamsData.h"
#include "PCGExRelationalData.generated.h"

class UPCGPointData;

// Detail stored in a attribute array
USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FPCGExRelationDetails
{
	GENERATED_BODY()

public:
	int64 Index = -1;
	double IndexedDot = -1.0;
	double IndexedDistance = TNumericLimits<double>::Max();

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FPCGExRelationDetails& Details)
	{
		Ar << Details.Index;
		Ar << Details.IndexedDot;
		Ar << Details.IndexedDistance;

		return Ar;
	}

	bool operator==(const FPCGExRelationDetails& Other) const
	{
		return Index == Other.Index && IndexedDot == Other.IndexedDot && IndexedDistance == Other.IndexedDistance;
	}
};

USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FPCGExRelationCandidate : public FPCGExRelationDirection
{
	GENERATED_BODY()

	FPCGExRelationCandidate()
	{
	}

	FPCGExRelationCandidate(const FPCGExRelationCandidate& Other):
		FPCGExRelationDirection(Other),
		Origin(Other.Origin),
		Index(Other.Index),
		IndexedDistance(Other.IndexedDistance),
		IndexedDot(Other.IndexedDot),
		DistanceScale(Other.DistanceScale)
	{
	}

	FPCGExRelationCandidate(const FPCGExRelationDirection& Other): FPCGExRelationDirection(Other)
	{
	}

	FPCGExRelationCandidate(const FPCGPoint& Point, const FPCGExRelationDefinition& Slot)
	{
		Direction = Slot.Direction.Direction;
		DotTolerance = Slot.Direction.DotTolerance;
		MaxDistance = Slot.Direction.MaxDistance;

		const FTransform PtTransform = Point.Transform;
		Origin = PtTransform.GetLocation();

		if (Slot.bRelativeOrientation)
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
	double DistanceScale = 1.0;

	double GetScaledDistance() const { return MaxDistance * DistanceScale; }

	bool ProcessPoint(const FPCGPoint* Point)
	{
		const double LocalMaxDistance = MaxDistance * DistanceScale;
		const FVector PtPosition = Point->Transform.GetLocation();
		const FVector DirToPt = (PtPosition - Origin).GetSafeNormal();

		const double SquaredDistance = FVector::DistSquared(Origin, PtPosition);

		// Is distance smaller than last registered one?
		if (SquaredDistance > IndexedDistance) { return false; }

		// Is distance inside threshold?
		if (SquaredDistance >= (LocalMaxDistance * LocalMaxDistance)) { return false; }

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

	FPCGExRelationDetails GetDetails() const
	{
		return FPCGExRelationDetails{Index, IndexedDot, IndexedDistance};
	}
};

// A temp data structure to be used during octree processing
USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FPCGExRelationData
{
	GENERATED_BODY()

	FPCGExRelationData()
	{
		Details.Empty();
	}

	FPCGExRelationData(int64 InIndex, const TArray<FPCGExRelationCandidate>* Candidates)
	{
		Index = InIndex;
		Details.Empty(Candidates->Num());
		for (const FPCGExRelationCandidate& Candidate : *Candidates)
		{
			if (Candidate.Index == InIndex)
			{
				Details.Add(FPCGExRelationDetails{});
			}
			else
			{
				Details.Add(Candidate.GetDetails());
			}
		}
	}

public:
	TArray<FPCGExRelationDetails> Details;
	int64 NumRelations = 0;
	int64 Index = -1;

	FPCGExRelationDetails operator [](int I) const { return Details[I]; }
	FPCGExRelationDetails& operator [](int I) { return Details[I]; }

	friend FArchive& operator<<(FArchive& Ar, FPCGExRelationData& Data)
	{
		Ar << Data.NumRelations;
		Ar << Data.Index;
		Ar << Data.Details;

		return Ar;
	}

	bool operator==(const FPCGExRelationData& Other) const
	{
		if (Details.Num() != Other.Details.Num()) { return false; }
		for (int i = 0; i < Details.Num(); i++)
		{
			if (Details[i] != Other.Details[i])
			{
				return false;
			}
		}
		return true;
	}

	FPCGExRelationData operator*(float Weight) const
	{
		FPCGExRelationData result;
		//TODO: Implement
		return result;
	}

	FPCGExRelationData operator*(const FPCGExRelationData Other) const
	{
		FPCGExRelationData result;
		//TODO: Implement
		return result;
	}

	FPCGExRelationData operator+(const FPCGExRelationData Other) const
	{
		FPCGExRelationData result;
		//TODO: Implement
		return result;
	}

	FPCGExRelationData operator-(const FPCGExRelationData Other) const
	{
		FPCGExRelationData result;
		//TODO: Implement
		return result;
	}

	FPCGExRelationData operator/(const FPCGExRelationData Other) const
	{
		FPCGExRelationData result;
		//TODO: Implement
		return result;
	}

	bool operator<(const FPCGExRelationData& Other) const
	{
		//TODO: Implement
		return false;
	}
};

template <typename T>
concept RelationalDataStruct = std::is_base_of_v<FPCGExRelationData, T>;

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExRelationalData : public UPCGPointData
{
	GENERATED_BODY()

public:
	UPCGExRelationalData(const FObjectInitializer& ObjectInitializer);

	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Param; }

	// Checks whether the data have the matching attribute
	bool IsDataReady(UPCGPointData* PointData);

public:
	UPROPERTY(BlueprintReadOnly)
	UPCGExRelationalParamsData* Params = nullptr;

	void Initialize(UPCGExRelationalParamsData** InParams);
	void Initialize(UPCGExRelationalData** InRelationalData);

	uint64 GetBoundUID() const { return Parent ? Parent->GetBoundUID() : BoundUID; }

	void SetBoundUID(uint64 NewBoundUID)
	{
#if WITH_EDITOR
		if (BoundUID != -1) { UE_LOG(LogTemp, Error, TEXT("RelationalData BoundUID overwritten; this is likely unwanted.")); }
#endif // WITH_EDITOR
		BoundUID = NewBoundUID;
	}

	TArray<FPCGExRelationData>& GetRelations() { return *Relations; }
	const FPCGExRelationData* GetRelation(int32 Index) const { return &(*Relations)[Index]; }

	double PrepareCandidatesForPoint(TArray<FPCGExRelationCandidate>& Candidates, const FPCGPoint& Point, bool bUseModifiers, TArray<FPCGExSamplingModifier>& Modifiers) const;

protected:
	UPROPERTY(Transient)
	uint64 BoundUID = -1;

	UPCGExRelationalData* Parent = nullptr;

	TArray<FPCGExRelationData> LocalRelations;
	TArray<FPCGExRelationData>* Relations;
};
