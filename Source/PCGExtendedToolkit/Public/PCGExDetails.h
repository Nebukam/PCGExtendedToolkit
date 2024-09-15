// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#define PCGEX_SOFT_VALIDATE_NAME_DETAILS(_BOOL, _NAME, _CTX) if(_BOOL){if (!FPCGMetadataAttributeBase::IsValidName(_NAME) || _NAME.IsNone()){ PCGE_LOG_C(Warning, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); _BOOL = false; } }

#include "CoreMinimal.h"
#include "PCGExMacros.h"
#include "PCGEx.h"
#include "PCGExActorSelector.h"
#include "PCGExMath.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExDetails.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Adjacency Direction Mode"))
enum class EPCGExAdjacencyDirectionOrigin : uint8
{
	FromNode     = 0 UMETA(DisplayName = "From Node to Neighbor", Tooltip="..."),
	FromNeighbor = 1 UMETA(DisplayName = "From Neighbor to Node", Tooltip="..."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Fetch Type"))
enum class EPCGExFetchType : uint8
{
	Constant  = 0 UMETA(DisplayName = "Constant", Tooltip="Constant."),
	Attribute = 1 UMETA(DisplayName = "Attribute", Tooltip="Attribute."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Fetch Type"))
enum class EPCGExPrune : uint8
{
	Overlap  = 0 UMETA(DisplayName = "Overlap", Tooltip="Prune if there is the slightest overlap."),
	Contains = 1 UMETA(DisplayName = "Contains", Tooltip="Prune if is fully contained by the target."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Data Filter Action"))
enum class EPCGExFilterDataAction : uint8
{
	Keep = 0 UMETA(DisplayName = "Keep", ToolTip="Keeps only selected data"),
	Omit = 1 UMETA(DisplayName = "Omit", ToolTip="Omit selected data from output"),
	Tag  = 2 UMETA(DisplayName = "Tag", ToolTip="Keep all and Tag"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Subdivide Mode"))
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
		PCGEX_CLEAN_SP(RemapCurveObj)
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
	double Tolerance = 0.001;

	/** Component-wise radiuses */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bComponentWiseTolerance", EditConditionHides, ClampMin=0.0001))
	FVector Tolerances = FVector(0.001);

	/** Method used to compute the distance from the source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalTolerance = false;

	/** Method used to compute the distance from the source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseLocalTolerance"))
	FPCGAttributePropertyInputSelector LocalTolerance;

	bool IsWithinTolerance(const double DistSquared) const
	{
		return FMath::IsWithin<double, double>(DistSquared, 0, Tolerance * Tolerance);
	}

	bool IsWithinTolerance(const FVector& Source, const FVector& Target) const
	{
		return FMath::IsWithin<double, double>(FVector::DistSquared(Source, Target), 0, Tolerance * Tolerance);
	}

	bool IsWithinToleranceComponentWise(const FVector& Source, const FVector& Target) const
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
	double GetSourceDistSquared(const FPCGPoint& SourcePoint, const FVector& SourceCenter, const FVector& TargetCenter) const
	{
		return FVector::DistSquared(PCGExMath::GetSpatializedCenter(SourceDistance, SourcePoint, SourceCenter, TargetCenter), TargetCenter);
	}

	bool IsWithinTolerance(const FPCGPoint& SourcePoint, const FVector& SourceCenter, const FVector& TargetCenter) const
	{
		return FPCGExFuseDetailsBase::IsWithinTolerance(PCGExMath::GetSpatializedCenter(SourceDistance, SourcePoint, SourceCenter, TargetCenter), TargetCenter);
	}

	bool IsWithinToleranceComponentWise(const FPCGPoint& SourcePoint, const FVector& SourceCenter, const FVector& TargetCenter) const
	{
		return FPCGExFuseDetailsBase::IsWithinToleranceComponentWise(PCGExMath::GetSpatializedCenter(SourceDistance, SourcePoint, SourceCenter, TargetCenter), TargetCenter);
	}
};


UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Fuse Precision"))
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
	EPCGExFuseMethod FuseMethod = EPCGExFuseMethod::Voxel;

	/** Offset the voxelized grid by an amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="FuseMethod==EPCGExFuseMethod::Voxel", EditConditionHides))
	FVector VoxelGridOffset = FVector::ZeroVector;

	/** Check this box if you're fusing over a very large radius and want to ensure determinism. NOTE : Will make things considerably slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="FuseMethod==EPCGExFuseMethod::Octree", EditConditionHides))
	bool bInlineInsertion = false;


	FVector CWTolerance = FVector::OneVector;

	void Init()
	{
		if (FuseMethod == EPCGExFuseMethod::Voxel)
		{
			Tolerances *= 2;
			Tolerance *= 2;
		}

		if (bComponentWiseTolerance) { CWTolerance = FVector(1 / Tolerances.X, 1 / Tolerances.Y, 1 / Tolerances.Z); }
		else { CWTolerance = FVector(1 / Tolerance); }
	}

	bool DoInlineInsertion() const { return FuseMethod == EPCGExFuseMethod::Octree && bInlineInsertion; }

	FORCEINLINE uint32 GetGridKey(const FVector& Location) const { return PCGEx::GH(Location + VoxelGridOffset, CWTolerance); }
	FORCEINLINE FBoxCenterAndExtent GetOctreeBox(const FVector& Location) const { return FBoxCenterAndExtent(Location, Tolerances); }

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
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPointPointIntersectionDetails
{
	GENERATED_BODY()

	/** Fuse Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExFuseDetails FuseDetails;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteCompounded = false;

	/** Name of the attribute to mark point as compounded or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable, EditCondition="bWriteCompounded"))
	FName CompoundedAttributeName = "bCompounded";

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteCompoundSize = false;

	/** Name of the attribute to mark the number of fused point held */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable, EditCondition="bWriteCompoundSize"))
	FName CompoundSizeAttributeName = "CompoundSize";
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPointEdgeIntersectionDetails
{
	GENERATED_BODY()

	/** If disabled, points will only check edges they aren't mapped to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	bool bEnableSelfIntersection = true;

	/** Fuse Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExSourceFuseDetails FuseDetails;

	/** When enabled, point will be moved exactly on the edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bSnapOnEdge = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteIntersector = false;

	/** Name of the attribute to flag point as intersector (result of an Point/Edge intersection) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable, EditCondition="bWriteIntersector"))
	FName IntersectorAttributeName = "bIntersector";

	void MakeSafeForTolerance(const double FuseTolerance)
	{
		FuseDetails.Tolerance = FMath::Clamp(FuseDetails.Tolerance, 0, FuseTolerance * 0.5);
		FuseDetails.Tolerances.X = FMath::Clamp(FuseDetails.Tolerances.X, 0, FuseTolerance * 0.5);
		FuseDetails.Tolerances.Y = FMath::Clamp(FuseDetails.Tolerances.Y, 0, FuseTolerance * 0.5);
		FuseDetails.Tolerances.Z = FMath::Clamp(FuseDetails.Tolerances.Z, 0, FuseTolerance * 0.5);
	}
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExEdgeEdgeIntersectionDetails
{
	GENERATED_BODY()

	/** If disabled, edges will only be checked against other datasets. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	bool bEnableSelfIntersection = true;

	/** Distance at which two edges are considered intersecting. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	double Tolerance = 0.001;
	double ToleranceSquared = 0.001;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseMinAngle = true;

	/** Min angle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseMinAngle", Units="Degrees", ClampMin=0, ClampMax=90))
	double MinAngle = 0;
	double MinDot = -1;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseMaxAngle = true;

	/** Maximum angle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseMaxAngle", Units="Degrees", ClampMin=0, ClampMax=90))
	double MaxAngle = 90;
	double MaxDot = 1;

	//

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteCrossing = false;

	/** Name of the attribute to flag point as crossing (result of an Edge/Edge intersection) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable, EditCondition="bWriteCrossing"))
	FName CrossingAttributeName = "bCrossing";

	/** Will copy the flag values of attributes from the edges onto the point in order to filter them. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable))
	bool bFlagCrossing = false;

	/** Name of an int32 flag to fetch from the first edge */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable, EditCondition="bFlagCrossing"))
	FName FlagA;

	/** Name of an int32 flag to fetch from the second edge */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable, EditCondition="bFlagCrossing"))
	FName FlagB;

	void Init()
	{
		MaxDot = bUseMinAngle ? PCGExMath::DegreesToDot(MinAngle) : 1;
		MinDot = bUseMaxAngle ? PCGExMath::DegreesToDot(MaxAngle) : -1;
		ToleranceSquared = Tolerance * Tolerance;
	}

	FORCEINLINE bool CheckDot(const double InDot) const { return InDot <= MaxDot && InDot >= MinDot; }
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

	void Init(FPCGContext* InContext)
	{
		World = InContext->SourceComponent->GetWorld();

		if (bIgnoreActors)
		{
			const TFunction<bool(const AActor*)> BoundsCheck = [](const AActor*) -> bool { return true; };
			const TFunction<bool(const AActor*)> SelfIgnoreCheck = [](const AActor*) -> bool { return true; };
			IgnoredActors = PCGExActorSelector::FindActors(IgnoredActorSelector, InContext->SourceComponent.Get(), BoundsCheck, SelfIgnoreCheck);
		}

		if (bIgnoreSelf) { IgnoredActors.Add(InContext->SourceComponent->GetOwner()); }
	}

	void Update(FCollisionQueryParams& InCollisionParams) const
	{
		InCollisionParams.bTraceComplex = bTraceComplex;
		InCollisionParams.AddIgnoredActors(IgnoredActors);
	}

	bool Linecast(const FVector& From, const FVector& To, FHitResult& HitResult) const
	{
		FCollisionQueryParams CollisionParams;
		Update(CollisionParams);

		switch (CollisionType)
		{
		case EPCGExCollisionFilterType::Channel:
			return World->LineTraceSingleByChannel(HitResult, From, To, CollisionChannel, CollisionParams);
		case EPCGExCollisionFilterType::ObjectType:
			return World->LineTraceSingleByObjectType(HitResult, From, To, FCollisionObjectQueryParams(CollisionObjectType), CollisionParams);
		case EPCGExCollisionFilterType::Profile:
			return World->LineTraceSingleByProfile(HitResult, From, To, CollisionProfileName, CollisionParams);
		default:
			return false;
		}
	}
};


namespace PCGExDetails
{
#pragma region Distance Settings

	static FPCGExDistanceDetails GetDistanceDetails(const EPCGExDistance InDistance)
	{
		return FPCGExDistanceDetails(InDistance, InDistance);
	}

	static FPCGExDistanceDetails GetDistanceDetails(const FPCGExPointPointIntersectionDetails& InSettings)
	{
		return FPCGExDistanceDetails(InSettings.FuseDetails.SourceDistance, InSettings.FuseDetails.TargetDistance);
	}

	static FPCGExDistanceDetails GetDistanceDetails(const FPCGExPointEdgeIntersectionDetails& InSettings)
	{
		return FPCGExDistanceDetails(InSettings.FuseDetails.SourceDistance, EPCGExDistance::Center);
	}

	static FPCGExDistanceDetails GetDistanceDetails(const FPCGExEdgeEdgeIntersectionDetails& InSettings)
	{
		return FPCGExDistanceDetails(EPCGExDistance::Center, EPCGExDistance::Center);
	}

#pragma endregion
}

#undef PCGEX_SOFT_VALIDATE_NAME_DETAILS
