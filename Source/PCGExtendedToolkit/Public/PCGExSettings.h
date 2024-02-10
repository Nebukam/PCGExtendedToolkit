// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExMath.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExSettings.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExClampSettings
{
	GENERATED_BODY()

	FPCGExClampSettings()
	{
	}

	FPCGExClampSettings(const FPCGExClampSettings& Other):
		bClampMin(Other.bClampMin),
		ClampMin(Other.ClampMin),
		bClampMax(Other.bClampMax),
		ClampMax(Other.ClampMax)
	{
	}

	/** Clamp minimum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bClampMin = false;

	/** Clamp minimum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bClampMin"))
	double ClampMin = 0;

	/** Clamp maximum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bClampMax = false;

	/** Clamp maximum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bClampMax"))
	double ClampMax = 0;

	double GetClampMin(const double InValue) const { return InValue < ClampMin ? ClampMin : InValue; }
	double GetClampMax(const double InValue) const { return InValue > ClampMax ? ClampMax : InValue; }
	double GetClampMinMax(const double InValue) const { return InValue > ClampMax ? ClampMax : InValue < ClampMin ? ClampMin : InValue; }

	double GetClampedValue(const double InValue) const
	{
		if (bClampMin && InValue < ClampMin) { return ClampMin; }
		if (bClampMax && InValue > ClampMax) { return ClampMax; }
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
		bInMin(Other.bInMin),
		InMin(Other.InMin),
		bInMax(Other.bInMax),
		InMax(Other.InMax),
		RangeMethod(Other.RangeMethod),
		Scale(Other.Scale),
		RemapCurveObj(Other.RemapCurveObj)
	{
	}

	~FPCGExRemapSettings()
	{
		RemapCurveObj = nullptr;
	}

	/** Fixed In Min value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bInMin = false;

	/** Fixed In Min value. If disabled, will use the lowest input value.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bInMin"))
	double InMin = 0;

	/** Fixed In Max value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bInMax = false;

	/** Fixed In Max value. If disabled, will use the highest input value.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bInMax"))
	double InMax = 0;

	/** How to remap before sampling the curve. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExRangeType RangeMethod = EPCGExRangeType::EffectiveRange;

	/** Scale output value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Scale = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UCurveFloat> RemapCurve = TSoftObjectPtr<UCurveFloat>(PCGEx::WeightDistributionLinear);

	TObjectPtr<UCurveFloat> RemapCurveObj = nullptr;

	void LoadCurve()
	{
		if (RemapCurve.IsNull()) { RemapCurveObj = TSoftObjectPtr<UCurveFloat>(PCGEx::WeightDistributionLinear).LoadSynchronous(); }
		else { RemapCurveObj = RemapCurve.LoadSynchronous(); }
	}

	double GetRemappedValue(double Value) const
	{
		return RemapCurveObj->GetFloatValue(PCGExMath::Remap(Value, InMin, InMax, 0, 1)) * Scale;
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
	FPCGExInputDescriptor LocalInfluence;

	/** If enabled, applies influence after each iteration; otherwise applies once at the end of the relaxing.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bProgressiveInfluence = true;
	
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExFuseSettings
{
	GENERATED_BODY()

	FPCGExFuseSettings()
	{
		Init();
	}

	FPCGExFuseSettings(double InTolerance)
		: Tolerance(InTolerance)
	{
		Init();
	}

	FPCGExFuseSettings(double InTolerance, EPCGExDistance SourceMethod)
		: Tolerance(InTolerance), SourceDistance(SourceMethod)
	{
		Init();
	}

	/** Fusing distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0.0001))
	double Tolerance = 0.001;
	double ToleranceSquared = 10;

	/** Uses a per-axis radius, manathan-style */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bComponentWiseTolerance = false;

	/** Component-wise radiuses */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bComponentWiseTolerance", EditConditionHides, ClampMin=0.0001))
	FVector Tolerances = FVector(0.001);

	/** Method used to compute the distance from the source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDistance SourceDistance = EPCGExDistance::Center;

	/** Method used to compute the distance from the source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalTolerance = false;

	/** Method used to compute the distance from the source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseLocalTolerance"))
	FPCGExInputDescriptor LocalTolerance;

	PCGEx::FLocalSingleFieldGetter* LocalToleranceGetter = nullptr;
	PCGEx::FLocalVectorGetter* LocalToleranceVectorGetter = nullptr;

	void Init()
	{
		ToleranceSquared = Tolerance * Tolerance;
		Tolerances = FVector(Tolerance);
	}

	bool IsWithinTolerance(const double DistSquared) const
	{
		return FMath::IsWithin<double, double>(DistSquared, 0, ToleranceSquared);
	}

	bool IsWithinTolerance(const FVector& Source, const FVector& Target) const
	{
		return FMath::IsWithin<double, double>(FVector::DistSquared(Source, Target), 0, ToleranceSquared);
	}

	bool IsWithinToleranceComponentWise(const FVector& Source, const FVector& Target) const
	{
		return (FMath::IsWithin<double, double>(abs(Source.X - Target.X), 0, Tolerance) &&
			FMath::IsWithin<double, double>(abs(Source.Y - Target.Y), 0, Tolerance) &&
			FMath::IsWithin<double, double>(abs(Source.Z - Target.Z), 0, Tolerance));
	}

	///
	FVector GetSourceCenter(const FPCGPoint& SourcePoint, const FVector& SourceCenter, const FVector& TargetCenter) const
	{
		return PCGExMath::GetRelationalCenter(SourceDistance, SourcePoint, SourceCenter, TargetCenter);
	}

	double GetSourceDistSquared(const FPCGPoint& SourcePoint, const FVector& SourceCenter, const FVector& TargetCenter) const
	{
		return FVector::DistSquared(PCGExMath::GetRelationalCenter(SourceDistance, SourcePoint, SourceCenter, TargetCenter), TargetCenter);
	}

	bool IsWithinTolerance(const FPCGPoint& SourcePoint, const FVector& SourceCenter, const FVector& TargetCenter) const
	{
		return IsWithinTolerance(GetSourceCenter(SourcePoint, SourceCenter, TargetCenter), TargetCenter);
	}

	bool IsWithinToleranceComponentWise(const FPCGPoint& SourcePoint, const FVector& SourceCenter, const FVector& TargetCenter) const
	{
		return IsWithinToleranceComponentWise(GetSourceCenter(SourcePoint, SourceCenter, TargetCenter), TargetCenter);
	}
};


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExFuseSettingsWithTarget : public FPCGExFuseSettings
{
	GENERATED_BODY()

	FPCGExFuseSettingsWithTarget()
	{
	}

	FPCGExFuseSettingsWithTarget(double InTolerance)
		: FPCGExFuseSettings(InTolerance)
	{
	}

	FPCGExFuseSettingsWithTarget(double InTolerance, EPCGExDistance SourceMethod)
		: FPCGExFuseSettings(InTolerance, SourceMethod)
	{
	}

	FPCGExFuseSettingsWithTarget(double InTolerance, EPCGExDistance SourceMethod, EPCGExDistance TargetMethod)
		: FPCGExFuseSettings(InTolerance, SourceMethod), TargetDistance(TargetMethod)
	{
	}

	/** Method used to compute the distance to the target */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDistance TargetDistance = EPCGExDistance::Center;

	void GetCenters(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const
	{
		OutSource = GetSourceCenter(SourcePoint, SourcePoint.Transform.GetLocation(), TargetPoint.Transform.GetLocation());
		OutTarget = PCGExMath::GetRelationalCenter(TargetDistance, TargetPoint, TargetPoint.Transform.GetLocation(), OutSource);
	}

	bool IsWithinTolerance(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint) const
	{
		FVector A;
		FVector B;
		GetCenters(SourcePoint, TargetPoint, A, B);
		return FPCGExFuseSettings::IsWithinTolerance(A, B);
	}

	bool IsWithinToleranceComponentWise(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint) const
	{
		FVector A;
		FVector B;
		GetCenters(SourcePoint, TargetPoint, A, B);
		return FPCGExFuseSettings::IsWithinToleranceComponentWise(A, B);
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExDistanceSettings
{
	GENERATED_BODY()

	FPCGExDistanceSettings()
	{
	}

	FPCGExDistanceSettings(EPCGExDistance SourceMethod)
		: SourceDistance(SourceMethod)
	{
	}

	/** Method used to compute the distance from the source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDistance SourceDistance = EPCGExDistance::Center;

	FVector GetSourceCenter(const FPCGPoint& SourcePoint, const FVector& SourceCenter, const FVector& TargetCenter) const
	{
		return PCGExMath::GetRelationalCenter(SourceDistance, SourcePoint, SourceCenter, TargetCenter);
	}

	double GetSourceDistanceToPosition(const FPCGPoint& SourcePoint, const FVector& SourceCenter, const FVector& TargetCenter) const
	{
		return FVector::DistSquared(GetSourceCenter(SourcePoint, SourceCenter, TargetCenter), TargetCenter);
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExDistanceSettingsWithTarget : public FPCGExDistanceSettings
{
	GENERATED_BODY()

	FPCGExDistanceSettingsWithTarget()
	{
	}

	FPCGExDistanceSettingsWithTarget(EPCGExDistance SourceMethod)
		: FPCGExDistanceSettings(SourceMethod)
	{
	}

	FPCGExDistanceSettingsWithTarget(EPCGExDistance SourceMethod, EPCGExDistance TargetMethod)
		: FPCGExDistanceSettings(SourceMethod), TargetDistance(TargetMethod)
	{
	}

	/** Method used to compute the distance to the target */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDistance TargetDistance = EPCGExDistance::Center;

	FVector GetTargetCenter(const FPCGPoint& TargetPoint, const FVector& TargetCenter, const FVector& OtherCenter) const
	{
		return PCGExMath::GetRelationalCenter(TargetDistance, TargetPoint, TargetCenter, OtherCenter);
	}

	void GetCenters(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const
	{
		const FVector TargetLocation = TargetPoint.Transform.GetLocation();
		OutSource = GetSourceCenter(SourcePoint, SourcePoint.Transform.GetLocation(), TargetLocation);
		OutTarget = GetTargetCenter(TargetPoint, TargetLocation, OutSource);
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExPointPointIntersectionSettings
{
	GENERATED_BODY()

	/** Fuse Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExFuseSettingsWithTarget FuseSettings;

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
