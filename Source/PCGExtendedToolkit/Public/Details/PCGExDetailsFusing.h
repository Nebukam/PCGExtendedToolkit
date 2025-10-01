﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "PCGExDetailsFusing.generated.h"

struct FPCGExContext;

namespace PCGExData
{
	struct FConstPoint;
	class FFacade;
}

namespace PCGExDetails
{
	class FDistances;

	template <typename T>
	class TSettingValue;
}


UENUM()
enum class EPCGExFuseMethod : uint8
{
	Voxel  = 0 UMETA(DisplayName = "Spatial Hash", Tooltip="Fast but blocky. Creates grid-looking approximation."),
	Octree = 1 UMETA(DisplayName = "Octree", Tooltip="Slow but precise. Respectful of the original topology. Requires stable insertion with large values."),
};


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExFuseDetailsBase
{
	GENERATED_BODY()

	FPCGExFuseDetailsBase();
	explicit FPCGExFuseDetailsBase(const bool InSupportLocalTolerance);
	FPCGExFuseDetailsBase(const bool InSupportLocalTolerance, const double InTolerance);

	virtual ~FPCGExFuseDetailsBase() = default;

	UPROPERTY(meta = (PCG_NotOverridable))
	bool bSupportLocalTolerance = false;

	/** Uses a per-axis radius, manathan-style */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bComponentWiseTolerance = false;

	/** Tolerance source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="bSupportLocalTolerance", EditConditionHides, HideEditConditionToggle))
	EPCGExInputValueType ToleranceInput = EPCGExInputValueType::Constant;

	/** Fusing distance attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName="Tolerance (Attr)", EditCondition="bSupportLocalTolerance && ToleranceInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector ToleranceAttribute;

	/** Fusing distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="ToleranceInput == EPCGExInputValueType::Constant && !bComponentWiseTolerance", EditConditionHides, ClampMin=0.0001))
	double Tolerance = DBL_COLLOCATION_TOLERANCE;

	/** Component-wise radiuses */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ToleranceInput == EPCGExInputValueType::Constant && bComponentWiseTolerance", EditConditionHides, ClampMin=0.0001))
	FVector Tolerances = FVector(DBL_COLLOCATION_TOLERANCE);

	virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade);

	bool IsWithinTolerance(const double DistSquared, const int32 PointIndex) const;
	bool IsWithinTolerance(const FVector& Source, const FVector& Target, const int32 PointIndex) const;
	bool IsWithinToleranceComponentWise(const FVector& Source, const FVector& Target, const int32 PointIndex) const;

protected:
	TSharedPtr<PCGExDetails::TSettingValue<FVector>> ToleranceGetter;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSourceFuseDetails : public FPCGExFuseDetailsBase
{
	GENERATED_BODY()

	FPCGExSourceFuseDetails();
	explicit FPCGExSourceFuseDetails(const bool InSupportLocalTolerance);
	FPCGExSourceFuseDetails(const bool InSupportLocalTolerance, const double InTolerance);
	explicit FPCGExSourceFuseDetails(const bool InSupportLocalTolerance, const double InTolerance, const EPCGExDistance SourceMethod);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDistance SourceDistance = EPCGExDistance::Center;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExFuseDetails : public FPCGExSourceFuseDetails
{
	GENERATED_BODY()

	FPCGExFuseDetails();

	explicit FPCGExFuseDetails(const bool InSupportLocalTolerance);
	FPCGExFuseDetails(const bool InSupportLocalTolerance, const double InTolerance);
	FPCGExFuseDetails(const bool InSupportLocalTolerance, const double InTolerance, const EPCGExDistance InSourceMethod);
	FPCGExFuseDetails(const bool InSupportLocalTolerance, const double InTolerance, const EPCGExDistance InSourceMethod, const EPCGExDistance InTargetMethod);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDistance TargetDistance = EPCGExDistance::Center;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFuseMethod FuseMethod = EPCGExFuseMethod::Voxel;

	/** Offset the voxelized grid by an amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="FuseMethod == EPCGExFuseMethod::Voxel", EditConditionHides))
	FVector VoxelGridOffset = FVector::ZeroVector;

	TSharedPtr<PCGExDetails::FDistances> DistanceDetails;

	/** Check this box if you're fusing over a very large radius and want to ensure insertion order to avoid snapping to different points. NOTE : Will make things considerably slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Stabilize Insertion Order"))
	bool bInlineInsertion = false;

	virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade) override;

	bool DoInlineInsertion() const { return bInlineInsertion; }

	uint32 GetGridKey(const FVector& Location, const int32 PointIndex) const;
	FBox GetOctreeBox(const FVector& Location, const int32 PointIndex) const;

	void GetCenters(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const;

	bool IsWithinTolerance(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint) const;
	bool IsWithinToleranceComponentWise(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint) const;
};
