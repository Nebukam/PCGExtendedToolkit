// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGParamData.h"
#include "PCGExRelationalData.generated.h"

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

// A setting group to be consumed by a PCGExRelationalData
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
		
		if (Ar.IsSaving())
		{	
			int32 NumDetails = Data.Details.Num();
			Ar << NumDetails;

			for (FPCGExRelationDetails& Detail : Data.Details)
			{
				Ar << Detail;
			}
		}
		else if (Ar.IsLoading()) // Check if deserializing
		{
			int32 NumDetails = 0;
			Ar << NumDetails;

			Data.Details.Empty(NumDetails);

			for (int32 i = 0; i < NumDetails; i++)
			{
				FPCGExRelationDetails& Detail = Data.Details.Emplace_GetRef();
				Ar << Detail;
			}
		}

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

	void InitializeFromSettings(const FPCGExRelationsDefinition& Definition);
	bool PrepareSelectors(const UPCGPointData* PointData, TArray<FPCGExSamplingModifier>& OutSelectors) const;
	double PrepareCandidatesForPoint(TArray<FPCGExRelationCandidate>& Candidates, const FPCGPoint& Point, bool bUseModifiers, TArray<FPCGExSamplingModifier>& Modifiers) const;

public:
	template <typename T, typename dummy = void>
	static double GetScaleFactor(const T& Value) { return static_cast<double>(Value); }

	template <typename dummy = void>
	static double GetScaleFactor(const FVector2D& Value) { return Value.Length(); }

	template <typename dummy = void>
	static double GetScaleFactor(const FVector& Value) { return Value.Length(); }

	template <typename dummy = void>
	static double GetScaleFactor(const FVector4& Value) { return FVector(Value).Length(); }

	template <typename dummy = void>
	static double GetScaleFactor(const FRotator& Value) { return 1.0; }

	template <typename dummy = void>
	static double GetScaleFactor(const FQuat& Value) { return 1.0; }

	template <typename dummy = void>
	static double GetScaleFactor(const FName& Value) { return 1.0; }

	template <typename dummy = void>
	static double GetScaleFactor(const FString& Value) { return 1.0; }

	template <typename dummy = void>
	static double GetScaleFactor(const FTransform& Value) { return 1.0; }
};
