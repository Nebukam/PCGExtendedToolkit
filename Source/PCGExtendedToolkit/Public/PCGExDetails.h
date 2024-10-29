// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMacros.h"
#include "PCGEx.h"
#include "PCGExMath.h"
#include "PCGExActorSelector.h"
#include "PCGExConstants.h"
#include "PCGExContext.h"

#include "PCGExDetails.generated.h"

#define PCGEX_SOFT_VALIDATE_NAME_DETAILS(_BOOL, _NAME, _CTX) if(_BOOL){if (!FPCGMetadataAttributeBase::IsValidName(_NAME) || _NAME.IsNone()){ PCGE_LOG_C(Warning, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); _BOOL = false; } }

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Adjacency Direction Mode")--E*/)
enum class EPCGExAdjacencyDirectionOrigin : uint8
{
	FromNode     = 0 UMETA(DisplayName = "From Node to Neighbor", Tooltip="..."),
	FromNeighbor = 1 UMETA(DisplayName = "From Neighbor to Node", Tooltip="..."),
};

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Fetch Type")--E*/)
enum class EPCGExInputValueType : uint8
{
	Constant  = 0 UMETA(DisplayName = "Constant", Tooltip="Constant."),
	Attribute = 1 UMETA(DisplayName = "Attribute", Tooltip="Attribute."),
};

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Fetch Type")--E*/)
enum class EPCGExPrune : uint8
{
	Overlap  = 0 UMETA(DisplayName = "Overlap", Tooltip="Prune if there is the slightest overlap."),
	Contains = 1 UMETA(DisplayName = "Contains", Tooltip="Prune if is fully contained by the target."),
};

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Data Filter Action")--E*/)
enum class EPCGExFilterDataAction : uint8
{
	Keep = 0 UMETA(DisplayName = "Keep", ToolTip="Keeps only selected data"),
	Omit = 1 UMETA(DisplayName = "Omit", ToolTip="Omit selected data from output"),
	Tag  = 2 UMETA(DisplayName = "Tag", ToolTip="Keep all and Tag"),
};

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Subdivide Mode")--E*/)
enum class EPCGExSubdivideMode : uint8
{
	Distance = 0 UMETA(DisplayName = "Distance", ToolTip="Number of subdivisions depends on segment' length"),
	Count    = 1 UMETA(DisplayName = "Count", ToolTip="Number of subdivisions is constant"),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExClampDetails
{
	GENERATED_BODY()

	FPCGExClampDetails()
	{
	}

	FPCGExClampDetails(const FPCGExClampDetails& Other):
		bApplyClampMin(Other.bApplyClampMin),
		ClampMinValue(Other.ClampMinValue),
		bApplyClampMax(Other.bApplyClampMax),
		ClampMaxValue(Other.ClampMaxValue)
	{
	}

	/** Clamp minimum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bApplyClampMin = false;

	/** Clamp minimum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bApplyClampMin"))
	double ClampMinValue = 0;

	/** Clamp maximum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bApplyClampMax = false;

	/** Clamp maximum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bApplyClampMax"))
	double ClampMaxValue = 0;

	FORCEINLINE double GetClampMin(const double InValue) const { return InValue < ClampMinValue ? ClampMinValue : InValue; }
	FORCEINLINE double GetClampMax(const double InValue) const { return InValue > ClampMaxValue ? ClampMaxValue : InValue; }
	FORCEINLINE double GetClampMinMax(const double InValue) const { return InValue > ClampMaxValue ? ClampMaxValue : InValue < ClampMinValue ? ClampMinValue : InValue; }

	FORCEINLINE double GetClampedValue(const double InValue) const
	{
		if (bApplyClampMin && InValue < ClampMinValue) { return ClampMinValue; }
		if (bApplyClampMax && InValue > ClampMaxValue) { return ClampMaxValue; }
		return InValue;
	}
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExRemapDetails
{
	GENERATED_BODY()

	FPCGExRemapDetails()
	{
	}

	FPCGExRemapDetails(const FPCGExRemapDetails& Other):
		bUseAbsoluteRange(Other.bUseAbsoluteRange),
		bPreserveSign(Other.bPreserveSign),
		bUseInMin(Other.bUseInMin),
		InMin(Other.InMin),
		CachedInMin(Other.InMin),
		bUseInMax(Other.bUseInMax),
		InMax(Other.InMax),
		CachedInMax(Other.InMax),
		RangeMethod(Other.RangeMethod),
		Scale(Other.Scale),
		RemapCurveObj(Other.RemapCurveObj),
		TruncateOutput(Other.TruncateOutput),
		PostTruncateScale(Other.PostTruncateScale)
	{
	}

	~FPCGExRemapDetails()
	{
	}

	/** Whether or not to use only positive values to compute range.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bUseAbsoluteRange = true;

	/** Whether or not to preserve value sign when using absolute range.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bUseAbsoluteRange"))
	bool bPreserveSign = true;

	/** Fixed In Min value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bUseInMin = false;

	/** Fixed In Min value. If disabled, will use the lowest input value.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bUseInMin"))
	double InMin = 0;
	double CachedInMin = 0;

	/** Fixed In Max value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bUseInMax = false;

	/** Fixed In Max value. If disabled, will use the highest input value.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bUseInMax"))
	double InMax = 0;
	double CachedInMax = 0;

	/** How to remap before sampling the curve. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExRangeType RangeMethod = EPCGExRangeType::EffectiveRange;

	/** Scale output value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Scale = 1;

	UPROPERTY(EditAnywhere, Category = Settings, BlueprintReadWrite)
	TSoftObjectPtr<UCurveFloat> RemapCurve = TSoftObjectPtr<UCurveFloat>(PCGEx::WeightDistributionLinear);

	UPROPERTY(Transient)
	TObjectPtr<UCurveFloat> RemapCurveObj = nullptr;


	/** Whether and how to truncate output value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExTruncateMode TruncateOutput = EPCGExTruncateMode::None;

	/** Scale the value after it's been truncated. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="TruncateOutput != EPCGExTruncateMode::None", EditConditionHides))
	double PostTruncateScale = 1;

	void LoadCurve()
	{
		PCGEX_LOAD_SOFTOBJECT(UCurveFloat, RemapCurve, RemapCurveObj, PCGEx::WeightDistributionLinear)
	}

	FORCEINLINE double GetRemappedValue(const double Value) const
	{
		double OutValue = RemapCurveObj->GetFloatValue(PCGExMath::Remap(Value, InMin, InMax, 0, 1)) * Scale;
		switch (TruncateOutput)
		{
		case EPCGExTruncateMode::Round:
			OutValue = FMath::RoundToInt(OutValue) * PostTruncateScale;
			break;
		case EPCGExTruncateMode::Ceil:
			OutValue = FMath::CeilToDouble(OutValue) * PostTruncateScale;
			break;
		case EPCGExTruncateMode::Floor:
			OutValue = FMath::FloorToDouble(OutValue) * PostTruncateScale;
			break;
		default:
		case EPCGExTruncateMode::None:
			return OutValue;
		}
		return OutValue;
	}
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExDistanceDetails
{
	GENERATED_BODY()

	FPCGExDistanceDetails()
	{
	}

	explicit FPCGExDistanceDetails(const EPCGExDistance SourceMethod, const EPCGExDistance TargetMethod)
		: Source(SourceMethod), Target(TargetMethod)
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	EPCGExDistance Source = EPCGExDistance::Center;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	EPCGExDistance Target = EPCGExDistance::Center;

	FORCEINLINE FVector GetSourceCenter(const FPCGPoint& FromPoint, const FVector& FromCenter, const FVector& ToCenter) const
	{
		return PCGExMath::GetSpatializedCenter(Source, FromPoint, FromCenter, ToCenter);
	}

	FORCEINLINE FVector GetTargetCenter(const FPCGPoint& FromPoint, const FVector& FromCenter, const FVector& ToCenter) const
	{
		return PCGExMath::GetSpatializedCenter(Target, FromPoint, FromCenter, ToCenter);
	}

	FORCEINLINE void GetCenters(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const
	{
		const FVector TargetLocation = TargetPoint.Transform.GetLocation();
		OutSource = PCGExMath::GetSpatializedCenter(Source, SourcePoint, SourcePoint.Transform.GetLocation(), TargetLocation);
		OutTarget = PCGExMath::GetSpatializedCenter(Target, TargetPoint, TargetLocation, OutSource);
	}

	FORCEINLINE double GetDistance(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint) const
	{
		const FVector TargetLocation = TargetPoint.Transform.GetLocation();
		const FVector OutSource = PCGExMath::GetSpatializedCenter(Source, SourcePoint, SourcePoint.Transform.GetLocation(), TargetLocation);
		return FVector::DistSquared(OutSource, PCGExMath::GetSpatializedCenter(Target, TargetPoint, TargetLocation, OutSource));
	}
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFuseDetailsBase
{
	GENERATED_BODY()

	FPCGExFuseDetailsBase()
	{
	}

	explicit FPCGExFuseDetailsBase(const double InTolerance)
		: Tolerance(InTolerance)
	{
	}

	/** Uses a per-axis radius, manathan-style */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bComponentWiseTolerance = false;

	/** Fusing distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bComponentWiseTolerance", EditConditionHides, ClampMin=0.0001))
	double Tolerance = DBL_COLLOCATION_TOLERANCE;

	/** Component-wise radiuses */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bComponentWiseTolerance", EditConditionHides, ClampMin=0.0001))
	FVector Tolerances = FVector(DBL_COLLOCATION_TOLERANCE);

	/** Method used to compute the distance from the source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalTolerance = false;

	/** Method used to compute the distance from the source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseLocalTolerance"))
	FPCGAttributePropertyInputSelector LocalTolerance;

	FORCEINLINE bool IsWithinTolerance(const double DistSquared) const
	{
		return FMath::IsWithin<double, double>(DistSquared, 0, Tolerance * Tolerance);
	}

	FORCEINLINE bool IsWithinTolerance(const FVector& Source, const FVector& Target) const
	{
		return FMath::IsWithin<double, double>(FVector::DistSquared(Source, Target), 0, Tolerance * Tolerance);
	}

	FORCEINLINE bool IsWithinToleranceComponentWise(const FVector& Source, const FVector& Target) const
	{
		return (FMath::IsWithin<double, double>(abs(Source.X - Target.X), 0, Tolerances.X) &&
			FMath::IsWithin<double, double>(abs(Source.Y - Target.Y), 0, Tolerances.Y) &&
			FMath::IsWithin<double, double>(abs(Source.Z - Target.Z), 0, Tolerances.Z));
	}
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSourceFuseDetails : public FPCGExFuseDetailsBase
{
	GENERATED_BODY()

	FPCGExSourceFuseDetails() :
		FPCGExFuseDetailsBase()
	{
	}

	explicit FPCGExSourceFuseDetails(const double InTolerance)
		: FPCGExFuseDetailsBase(InTolerance)
	{
	}

	explicit FPCGExSourceFuseDetails(const double InTolerance, const EPCGExDistance SourceMethod)
		: FPCGExFuseDetailsBase(InTolerance), SourceDistance(SourceMethod)
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDistance SourceDistance = EPCGExDistance::Center;

	///
	FORCEINLINE double GetSourceDistSquared(const FPCGPoint& SourcePoint, const FVector& SourceCenter, const FVector& TargetCenter) const
	{
		return FVector::DistSquared(PCGExMath::GetSpatializedCenter(SourceDistance, SourcePoint, SourceCenter, TargetCenter), TargetCenter);
	}

	FORCEINLINE bool IsWithinTolerance(const FPCGPoint& SourcePoint, const FVector& SourceCenter, const FVector& TargetCenter) const
	{
		return FPCGExFuseDetailsBase::IsWithinTolerance(PCGExMath::GetSpatializedCenter(SourceDistance, SourcePoint, SourceCenter, TargetCenter), TargetCenter);
	}

	FORCEINLINE bool IsWithinToleranceComponentWise(const FPCGPoint& SourcePoint, const FVector& SourceCenter, const FVector& TargetCenter) const
	{
		return FPCGExFuseDetailsBase::IsWithinToleranceComponentWise(PCGExMath::GetSpatializedCenter(SourceDistance, SourcePoint, SourceCenter, TargetCenter), TargetCenter);
	}
};


UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Fuse Precision")--E*/)
enum class EPCGExFuseMethod : uint8
{
	Voxel  = 0 UMETA(DisplayName = "Voxel", Tooltip="Fast but blocky. Creates grid-looking approximation.Destructive toward initial topology."),
	Octree = 1 UMETA(DisplayName = "Octree", Tooltip="Slow but precise. Respectful of the original topology."),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFuseDetails : public FPCGExSourceFuseDetails
{
	GENERATED_BODY()

	FPCGExFuseDetails() :
		FPCGExSourceFuseDetails()
	{
	}

	explicit FPCGExFuseDetails(const double InTolerance)
		: FPCGExSourceFuseDetails(InTolerance)
	{
	}

	explicit FPCGExFuseDetails(const double InTolerance, const EPCGExDistance SourceMethod)
		: FPCGExSourceFuseDetails(InTolerance, SourceMethod)
	{
	}

	explicit FPCGExFuseDetails(const double InTolerance, const EPCGExDistance SourceMethod, const EPCGExDistance TargetMethod)
		: FPCGExSourceFuseDetails(InTolerance, SourceMethod)
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDistance TargetDistance = EPCGExDistance::Center;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFuseMethod FuseMethod = EPCGExFuseMethod::Octree;

	/** Offset the voxelized grid by an amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="FuseMethod==EPCGExFuseMethod::Voxel", EditConditionHides))
	FVector VoxelGridOffset = FVector::ZeroVector;

	/** Check this box if you're fusing over a very large radius and want to ensure determinism. NOTE : Will make things slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Force Determinism", EditCondition="FuseMethod==EPCGExFuseMethod::Octree", EditConditionHides))
	bool bInlineInsertion = false;


	FVector CWTolerance = FVector::OneVector;

	void Init()
	{
		if (FuseMethod == EPCGExFuseMethod::Voxel)
		{
			Tolerances *= 2;
			Tolerance *= 2;

			if (bComponentWiseTolerance) { CWTolerance = FVector(1 / Tolerances.X, 1 / Tolerances.Y, 1 / Tolerances.Z); }
			else { CWTolerance = FVector(1 / Tolerance); }
		}
		else
		{
			if (bComponentWiseTolerance) { CWTolerance = Tolerances; }
			else { CWTolerance = FVector(Tolerance); }
		}
	}

	bool DoInlineInsertion() const { return FuseMethod == EPCGExFuseMethod::Octree && bInlineInsertion; }

	FORCEINLINE uint32 GetGridKey(const FVector& Location) const { return PCGEx::GH(Location + VoxelGridOffset, CWTolerance); }
	FORCEINLINE FBoxCenterAndExtent GetOctreeBox(const FVector& Location) const { return FBoxCenterAndExtent(Location, CWTolerance); }

	FORCEINLINE void GetCenters(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const
	{
		OutSource = PCGExMath::GetSpatializedCenter(SourceDistance, SourcePoint, SourcePoint.Transform.GetLocation(), TargetPoint.Transform.GetLocation());
		OutTarget = PCGExMath::GetSpatializedCenter(TargetDistance, TargetPoint, TargetPoint.Transform.GetLocation(), OutSource);
	}

	FORCEINLINE bool IsWithinTolerance(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint) const
	{
		FVector A;
		FVector B;
		GetCenters(SourcePoint, TargetPoint, A, B);
		return FPCGExFuseDetailsBase::IsWithinTolerance(A, B);
	}

	FORCEINLINE bool IsWithinToleranceComponentWise(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint) const
	{
		FVector A;
		FVector B;
		GetCenters(SourcePoint, TargetPoint, A, B);
		return FPCGExFuseDetailsBase::IsWithinToleranceComponentWise(A, B);
	}
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTransformDetails
{
	GENERATED_BODY()

	/** If enabled, copied point will be scaled by the target' scale. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInheritScale = false;

	/** If enabled, copied points will be rotated by the target' rotation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInheritRotation = false;
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCollisionDetails
{
	GENERATED_BODY()

	FPCGExCollisionDetails()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bTraceComplex = false;

	/** Collision type to check against */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExCollisionFilterType CollisionType = EPCGExCollisionFilterType::Channel;

	/** Collision channel to check against */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CollisionType==EPCGExCollisionFilterType::Channel", EditConditionHides, Bitmask, BitmaskEnum="/Script/Engine.ECollisionChannel"))
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_WorldDynamic;

	/** Collision object type to check against */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CollisionType==EPCGExCollisionFilterType::ObjectType", EditConditionHides, Bitmask, BitmaskEnum="/Script/Engine.EObjectTypeQuery"))
	int32 CollisionObjectType = ObjectTypeQuery1;

	/** Collision profile to check against */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CollisionType==EPCGExCollisionFilterType::Profile", EditConditionHides))
	FName CollisionProfileName = NAME_None;

	/** Ignore this graph' PCG content */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bIgnoreSelf = true;

	/** Ignore a procedural selection of actors */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bIgnoreActors = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bIgnoreActors"))
	FPCGExActorSelectorSettings IgnoredActorSelector;

	TArray<AActor*> IgnoredActors;

	UWorld* World = nullptr;

	void Init(const FPCGExContext* InContext);
	void Update(FCollisionQueryParams& InCollisionParams) const;
	bool Linecast(const FVector& From, const FVector& To, FHitResult& HitResult) const;
};


namespace PCGExDetails
{
#pragma region Distance Settings

	static FPCGExDistanceDetails GetDistanceDetails(const EPCGExDistance InDistance)
	{
		return FPCGExDistanceDetails(InDistance, InDistance);
	}

#pragma endregion
}

#undef PCGEX_SOFT_VALIDATE_NAME_DETAILS
