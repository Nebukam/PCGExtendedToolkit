// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#define PCGEX_SOFT_VALIDATE_NAME_SETTINGS(_BOOL, _NAME, _CTX) if(_BOOL){if (!FPCGMetadataAttributeBase::IsValidName(_NAME) || _NAME.IsNone()){ PCGE_LOG_C(Warning, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); _BOOL = false; } }

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExMath.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Kismet/KismetMathLibrary.h"

#include "PCGExSettings.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Fetch Type"))
enum class EPCGExFetchType : uint8
{
	Constant UMETA(DisplayName = "Constant", Tooltip="Constant."),
	Attribute UMETA(DisplayName = "Attribute", Tooltip="Attribute."),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExClampSettings
{
	GENERATED_BODY()

	FPCGExClampSettings()
	{
	}

	FPCGExClampSettings(const FPCGExClampSettings& Other):
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

	double GetClampMin(const double InValue) const { return InValue < ClampMinValue ? ClampMinValue : InValue; }
	double GetClampMax(const double InValue) const { return InValue > ClampMaxValue ? ClampMaxValue : InValue; }
	double GetClampMinMax(const double InValue) const { return InValue > ClampMaxValue ? ClampMaxValue : InValue < ClampMinValue ? ClampMinValue : InValue; }

	FORCEINLINE double GetClampedValue(const double InValue) const
	{
		if (bApplyClampMin && InValue < ClampMinValue) { return ClampMinValue; }
		if (bApplyClampMax && InValue > ClampMaxValue) { return ClampMaxValue; }
		return InValue;
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExRemapSettings
{
	GENERATED_BODY()

	FPCGExRemapSettings()
	{
	}

	FPCGExRemapSettings(const FPCGExRemapSettings& Other):
		bUseAbsoluteRange(Other.bUseAbsoluteRange),
		bPreserveSign(Other.bPreserveSign),
		bUseInMin(Other.bUseInMin),
		InMin(Other.InMin),
		bUseInMax(Other.bUseInMax),
		InMax(Other.InMax),
		RangeMethod(Other.RangeMethod),
		Scale(Other.Scale),
		RemapCurveObj(Other.RemapCurveObj),
		TruncateOutput(Other.TruncateOutput),
		PostTruncateScale(Other.PostTruncateScale)
	{
	}

	~FPCGExRemapSettings()
	{
		RemapCurveObj = nullptr;
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

	/** Fixed In Max value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bUseInMax = false;

	/** Fixed In Max value. If disabled, will use the highest input value.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bUseInMax"))
	double InMax = 0;

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
		}
		return OutValue;
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInfluenceSettings
{
	GENERATED_BODY()

	FPCGExInfluenceSettings()
	{
	}

	/** Draw size. What it means depends on the selected debug type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=-1, ClampMax=1))
	double Influence = 1.0;

	/** Fetch the size from a local attribute. The regular Size parameter then act as a scale.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalInfluence = false;

	/** Fetch the size from a local attribute. The regular Size parameter then act as a scale.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bUseLocalInfluence"))
	FPCGAttributePropertyInputSelector LocalInfluence;

	/** If enabled, applies influence after each iteration; otherwise applies once at the end of the relaxing.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bProgressiveInfluence = true;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExDistanceSettings
{
	GENERATED_BODY()

	FPCGExDistanceSettings()
	{
	}

	explicit FPCGExDistanceSettings(const EPCGExDistance SourceMethod, const EPCGExDistance TargetMethod)
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
struct PCGEXTENDEDTOOLKIT_API FPCGExFuseSettingsBase
{
	GENERATED_BODY()

	FPCGExFuseSettingsBase()
	{
	}

	explicit FPCGExFuseSettingsBase(const double InTolerance)
		: Tolerance(InTolerance)
	{
	}

	/** Fusing distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0.0001))
	double Tolerance = 0.001;

	/** Uses a per-axis radius, manathan-style */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bComponentWiseTolerance = false;

	/** Component-wise radiuses */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bComponentWiseTolerance", EditConditionHides, ClampMin=0.0001))
	FVector Tolerances = FVector(0.001);

	/** Method used to compute the distance from the source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalTolerance = false;

	/** Method used to compute the distance from the source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseLocalTolerance"))
	FPCGAttributePropertyInputSelector LocalTolerance;

	PCGEx::FLocalSingleFieldGetter* LocalToleranceGetter = nullptr;
	PCGEx::FLocalVectorGetter* LocalToleranceVectorGetter = nullptr;

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
		return (FMath::IsWithin<double, double>(abs(Source.X - Target.X), 0, Tolerance) &&
			FMath::IsWithin<double, double>(abs(Source.Y - Target.Y), 0, Tolerance) &&
			FMath::IsWithin<double, double>(abs(Source.Z - Target.Z), 0, Tolerance));
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSourceFuseSettings : public FPCGExFuseSettingsBase
{
	GENERATED_BODY()

	FPCGExSourceFuseSettings() :
		FPCGExFuseSettingsBase()
	{
	}

	explicit FPCGExSourceFuseSettings(const double InTolerance)
		: FPCGExFuseSettingsBase(InTolerance)
	{
	}

	explicit FPCGExSourceFuseSettings(const double InTolerance, const EPCGExDistance SourceMethod)
		: FPCGExFuseSettingsBase(InTolerance), SourceDistance(SourceMethod)
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
		return FPCGExFuseSettingsBase::IsWithinTolerance(PCGExMath::GetSpatializedCenter(SourceDistance, SourcePoint, SourceCenter, TargetCenter), TargetCenter);
	}

	bool IsWithinToleranceComponentWise(const FPCGPoint& SourcePoint, const FVector& SourceCenter, const FVector& TargetCenter) const
	{
		return FPCGExFuseSettingsBase::IsWithinToleranceComponentWise(PCGExMath::GetSpatializedCenter(SourceDistance, SourcePoint, SourceCenter, TargetCenter), TargetCenter);
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExFuseSettings : public FPCGExSourceFuseSettings
{
	GENERATED_BODY()

	FPCGExFuseSettings() :
		FPCGExSourceFuseSettings()
	{
	}

	explicit FPCGExFuseSettings(const double InTolerance)
		: FPCGExSourceFuseSettings(InTolerance)
	{
	}

	explicit FPCGExFuseSettings(const double InTolerance, const EPCGExDistance SourceMethod)
		: FPCGExSourceFuseSettings(InTolerance, SourceMethod)
	{
	}

	explicit FPCGExFuseSettings(const double InTolerance, const EPCGExDistance SourceMethod, const EPCGExDistance TargetMethod)
		: FPCGExSourceFuseSettings(InTolerance, SourceMethod)
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDistance TargetDistance = EPCGExDistance::Center;

	void GetCenters(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const
	{
		OutSource = PCGExMath::GetSpatializedCenter(SourceDistance, SourcePoint, SourcePoint.Transform.GetLocation(), TargetPoint.Transform.GetLocation());
		OutTarget = PCGExMath::GetSpatializedCenter(TargetDistance, TargetPoint, TargetPoint.Transform.GetLocation(), OutSource);
	}

	bool IsWithinTolerance(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint) const
	{
		FVector A;
		FVector B;
		GetCenters(SourcePoint, TargetPoint, A, B);
		return FPCGExFuseSettingsBase::IsWithinTolerance(A, B);
	}

	bool IsWithinToleranceComponentWise(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint) const
	{
		FVector A;
		FVector B;
		GetCenters(SourcePoint, TargetPoint, A, B);
		return FPCGExFuseSettingsBase::IsWithinToleranceComponentWise(A, B);
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExPointPointIntersectionSettings
{
	GENERATED_BODY()

	/** Fuse Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExFuseSettings FuseSettings;

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
struct PCGEXTENDEDTOOLKIT_API FPCGExPointEdgeIntersectionSettings
{
	GENERATED_BODY()

	/** If disabled, points will only check edges they aren't mapped to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	bool bEnableSelfIntersection = true;

	/** Fuse Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExSourceFuseSettings FuseSettings;

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
		FuseSettings.Tolerance = FMath::Clamp(FuseSettings.Tolerance, 0, FuseTolerance * 0.5);
		FuseSettings.Tolerances.X = FMath::Clamp(FuseSettings.Tolerances.X, 0, FuseTolerance * 0.5);
		FuseSettings.Tolerances.Y = FMath::Clamp(FuseSettings.Tolerances.Y, 0, FuseTolerance * 0.5);
		FuseSettings.Tolerances.Z = FMath::Clamp(FuseSettings.Tolerances.Z, 0, FuseTolerance * 0.5);
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExEdgeEdgeIntersectionSettings
{
	GENERATED_BODY()

	/** If disabled, edges will only be checked against other datasets. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	bool bEnableSelfIntersection = true;

	/** Distance at which two edges are considered intersecting. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	double Tolerance = 0.001;

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

	void MakeSafeForTolerance(const double FuseTolerance)
	{
		Tolerance = FMath::Clamp(Tolerance, 0, FuseTolerance * 0.5);
	}

	void ComputeDot()
	{
		MinDot = bUseMinAngle ? PCGExMath::DegreesToDot(MinAngle) : -1;
		MaxDot = bUseMaxAngle ? PCGExMath::DegreesToDot(MaxAngle) : 1;
	}
};

namespace PCGExSettings
{
#pragma region Distance Settings

	static FPCGExDistanceSettings GetDistanceSettings(const EPCGExDistance InDistance)
	{
		return FPCGExDistanceSettings(InDistance, InDistance);
	}

	static FPCGExDistanceSettings GetDistanceSettings(const FPCGExPointPointIntersectionSettings& InSettings)
	{
		return FPCGExDistanceSettings(InSettings.FuseSettings.SourceDistance, InSettings.FuseSettings.TargetDistance);
	}

	static FPCGExDistanceSettings GetDistanceSettings(const FPCGExPointEdgeIntersectionSettings& InSettings)
	{
		return FPCGExDistanceSettings(InSettings.FuseSettings.SourceDistance, EPCGExDistance::Center);
	}

	static FPCGExDistanceSettings GetDistanceSettings(const FPCGExEdgeEdgeIntersectionSettings& InSettings)
	{
		return FPCGExDistanceSettings(EPCGExDistance::Center, EPCGExDistance::Center);
	}

#pragma endregion
}

#undef PCGEX_SOFT_VALIDATE_NAME_SETTINGS
