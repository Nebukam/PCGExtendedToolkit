// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGParamData.h"
#include "PCGExRelationalData.generated.h"

class UPCGPointData;

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

	/** Whether to apply point rotation to the sampling direction. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bApplyPointRotation = true;

	/** What kind of distance modulation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bModulateDistance = false;

	/** Name of the attribute to compare */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bModulateDistance", EditConditionHides))
	FPCGExSelectorSettingsBase ModulationAttribute;

	/** Whether this slot is enabled or not. Handy to do trial-and-error without adding/deleting array elements. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(AdvancedDisplay))
	bool bEnabled = true;

	/** Debug color. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(AdvancedDisplay))
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
		IndexedDot(Other.IndexedDot),
		DistanceScale(Other.DistanceScale)
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

		if (Slot.bApplyPointRotation)
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

	double GetScaledDistance() { return MaxDistance * DistanceScale; }

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
		FPCGExRelationalSlot{"Forward", FPCGExSamplingDirection{FVector::ForwardVector}, true, false, FPCGExSelectorSettingsBase{}, true, FColor(255, 0, 0)},
		FPCGExRelationalSlot{"Backward", FPCGExSamplingDirection{FVector::BackwardVector}, true, false, FPCGExSelectorSettingsBase{}, true, FColor(200, 0, 0)},
		FPCGExRelationalSlot{"Right", FPCGExSamplingDirection{FVector::RightVector}, true, false, FPCGExSelectorSettingsBase{}, true, FColor(0, 255, 0)},
		FPCGExRelationalSlot{"Left", FPCGExSamplingDirection{FVector::LeftVector}, true, false, FPCGExSelectorSettingsBase{}, true, FColor(0, 200, 0)},
		FPCGExRelationalSlot{"Up", FPCGExSamplingDirection{FVector::UpVector}, true, false, FPCGExSelectorSettingsBase{}, true, FColor(0, 0, 255)},
		FPCGExRelationalSlot{"Down", FPCGExSamplingDirection{FVector::DownVector}, true, false, FPCGExSelectorSettingsBase{}, true, FColor(0, 0, 200)}
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
	
	FPCGExRelationAttributeData(int64 InIndex, const TArray<FPCGExSamplingCandidateData>* Candidates)
	{
		Indices.Empty();
		for (const FPCGExSamplingCandidateData& Candidate : *Candidates)
		{
			Indices.Add(Candidate.Index);
		}
		Index = InIndex;
	}

public:
	TArray<int64> Indices;
	TSet<int64> Mutual;
	int64 NumRelations = 0;
	int64 Index = -1;

	friend FArchive& operator<<(FArchive& Ar, FPCGExRelationAttributeData& SlotAttData)
	{
		// Implement serialization and deserialization here
		//Ar << SlotAttData.Index;		
		return Ar;
	}

	void MarkMutual(int64 MutualIndex) { Mutual.Add(MutualIndex); }

	/* Note: this assume that "Other" is already known to have mutual connection with this one.*/
	bool MarkMutuals(FPCGExRelationAttributeData* Other)
	{
		if (Indices.Contains(Other->Index))
		{
			MarkMutual(Other->Index);
			Other->MarkMutual(Index);
			return true;
		}
		return false;
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
	double GreatestStaticMaxDistance = 0.0;

	UPROPERTY(BlueprintReadOnly)
	bool bMarkMutualRelations = true;

	UPROPERTY(BlueprintReadOnly)
	bool bHasVariableMaxDistance = false;

	void InitializeLocalDefinition(const FPCGExRelationsDefinition& Definition);
	bool PrepareModifiers(const UPCGPointData* PointData, TArray<FPCGExSelectorSettingsBase>& OutModifiers) const;
	double InitializeCandidatesForPoint(TArray<FPCGExSamplingCandidateData>& Candidates, const FPCGPoint& Point, bool bUseModifiers, TArray<FPCGExSelectorSettingsBase>& Modifiers) const;

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
