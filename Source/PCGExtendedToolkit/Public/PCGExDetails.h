// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMacros.h"
#include "PCGEx.h"
#include "PCGExMath.h"
#include "PCGExActorSelector.h"

#include "PCGExContext.h"

#include "PCGExDetails.generated.h"

#define PCGEX_SOFT_VALIDATE_NAME_DETAILS(_BOOL, _NAME, _CTX) if(_BOOL){if (!FPCGMetadataAttributeBase::IsValidName(_NAME) || _NAME.IsNone()){ PCGE_LOG_C(Warning, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); _BOOL = false; } }

UENUM()
enum class EPCGExInputValueType : uint8
{
	Constant  = 0 UMETA(DisplayName = "Constant", Tooltip="Constant."),
	Attribute = 1 UMETA(DisplayName = "Attribute", Tooltip="Attribute."),
};

UENUM()
enum class EPCGExFilterDataAction : uint8
{
	Keep = 0 UMETA(DisplayName = "Keep", ToolTip="Keeps only selected data"),
	Omit = 1 UMETA(DisplayName = "Omit", ToolTip="Omit selected data from output"),
	Tag  = 2 UMETA(DisplayName = "Tag", ToolTip="Keep all and Tag"),
};

UENUM()
enum class EPCGExSubdivideMode : uint8
{
	Distance = 0 UMETA(DisplayName = "Distance", ToolTip="Number of subdivisions depends on length"),
	Count    = 1 UMETA(DisplayName = "Count", ToolTip="Number of subdivisions is fixed"),
};

namespace PCGExDetails
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FDistances : public TSharedFromThis<FDistances>
	{
	public:
		virtual ~FDistances() = default;

		bool bOverlapIsZero = false;

		FDistances()
		{
		}

		explicit FDistances(const bool InOverlapIsZero)
			: bOverlapIsZero(InOverlapIsZero)
		{
		}

		FORCEINLINE virtual FVector GetSourceCenter(const FPCGPoint& OriginPoint, const FVector& OriginLocation, const FVector& ToCenter) const = 0;
		FORCEINLINE virtual FVector GetTargetCenter(const FPCGPoint& OriginPoint, const FVector& OriginLocation, const FVector& ToCenter) const = 0;
		FORCEINLINE virtual void GetCenters(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const = 0;
		FORCEINLINE virtual double GetDistSquared(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint) const = 0;
		FORCEINLINE virtual double GetDist(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint) const = 0;
		FORCEINLINE virtual double GetDistSquared(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, bool& bOverlap) const = 0;
		FORCEINLINE virtual double GetDist(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, bool& bOverlap) const = 0;
	};

	template <EPCGExDistance Source, EPCGExDistance Target>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDistances final : public FDistances
	{
	public:
		TDistances()
		{
		}

		explicit TDistances(const bool InOverlapIsZero)
			: FDistances(InOverlapIsZero)
		{
		}

		FORCEINLINE virtual FVector GetSourceCenter(const FPCGPoint& FromPoint, const FVector& FromCenter, const FVector& ToCenter) const override
		{
			return PCGExMath::GetSpatializedCenter<Source>(FromPoint, FromCenter, ToCenter);
		}

		FORCEINLINE virtual FVector GetTargetCenter(const FPCGPoint& FromPoint, const FVector& FromCenter, const FVector& ToCenter) const override
		{
			return PCGExMath::GetSpatializedCenter<Target>(FromPoint, FromCenter, ToCenter);
		}

		FORCEINLINE virtual void GetCenters(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const override
		{
			const FVector TargetOrigin = TargetPoint.Transform.GetLocation();
			OutSource = PCGExMath::GetSpatializedCenter<Source>(SourcePoint, SourcePoint.Transform.GetLocation(), TargetOrigin);
			OutTarget = PCGExMath::GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource);
		}

		FORCEINLINE virtual double GetDistSquared(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint) const override
		{
			const FVector TargetOrigin = TargetPoint.Transform.GetLocation();
			const FVector OutSource = PCGExMath::GetSpatializedCenter<Source>(SourcePoint, SourcePoint.Transform.GetLocation(), TargetOrigin);
			return FVector::DistSquared(OutSource, PCGExMath::GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource));
		}

		FORCEINLINE virtual double GetDist(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint) const override
		{
			const FVector TargetOrigin = TargetPoint.Transform.GetLocation();
			const FVector OutSource = PCGExMath::GetSpatializedCenter<Source>(SourcePoint, SourcePoint.Transform.GetLocation(), TargetOrigin);
			return FVector::Dist(OutSource, PCGExMath::GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource));
		}

		FORCEINLINE virtual double GetDistSquared(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, bool& bOverlap) const override
		{
			const FVector TargetOrigin = TargetPoint.Transform.GetLocation();
			const FVector SourceOrigin = SourcePoint.Transform.GetLocation();
			const FVector OutSource = PCGExMath::GetSpatializedCenter<Source>(SourcePoint, SourceOrigin, TargetOrigin);
			const FVector OutTarget = PCGExMath::GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource);

			bOverlap = FVector::DotProduct((TargetOrigin - SourceOrigin), (OutTarget - OutSource)) < 0;
			return FVector::DistSquared(OutSource, OutTarget);
		}

		FORCEINLINE virtual double GetDist(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, bool& bOverlap) const override
		{
			const FVector TargetOrigin = TargetPoint.Transform.GetLocation();
			const FVector SourceOrigin = SourcePoint.Transform.GetLocation();
			const FVector OutSource = PCGExMath::GetSpatializedCenter<Source>(SourcePoint, SourceOrigin, TargetOrigin);
			const FVector OutTarget = PCGExMath::GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource);

			bOverlap = FVector::DotProduct((TargetOrigin - SourceOrigin), (OutTarget - OutSource)) < 0;
			return FVector::Dist(OutSource, OutTarget);
		}
	};


	static TSharedPtr<FDistances> MakeDistances(
		const EPCGExDistance Source = EPCGExDistance::Center,
		const EPCGExDistance Target = EPCGExDistance::Center,
		const bool bOverlapIsZero = false)
	{
		if (Source == EPCGExDistance::None || Target == EPCGExDistance::None)
		{
			return MakeShared<TDistances<EPCGExDistance::None, EPCGExDistance::None>>();
		}
		if (Source == EPCGExDistance::Center)
		{
			if (Target == EPCGExDistance::Center) { return MakeShared<TDistances<EPCGExDistance::Center, EPCGExDistance::Center>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::SphereBounds) { return MakeShared<TDistances<EPCGExDistance::Center, EPCGExDistance::SphereBounds>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::BoxBounds) { return MakeShared<TDistances<EPCGExDistance::Center, EPCGExDistance::BoxBounds>>(bOverlapIsZero); }
		}
		else if (Source == EPCGExDistance::SphereBounds)
		{
			if (Target == EPCGExDistance::Center) { return MakeShared<TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::Center>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::SphereBounds) { return MakeShared<TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::SphereBounds>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::BoxBounds) { return MakeShared<TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::BoxBounds>>(bOverlapIsZero); }
		}
		else if (Source == EPCGExDistance::BoxBounds)
		{
			if (Target == EPCGExDistance::Center) { return MakeShared<TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::Center>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::SphereBounds) { return MakeShared<TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::SphereBounds>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::BoxBounds) { return MakeShared<TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::BoxBounds>>(bOverlapIsZero); }
		}

		return nullptr;
	}

	static TSharedPtr<FDistances> MakeNoneDistances()
	{
		return MakeShared<TDistances<EPCGExDistance::None, EPCGExDistance::None>>();
	}
}

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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDistance Source = EPCGExDistance::Center;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDistance Target = EPCGExDistance::Center;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bOverlapIsZero = true;

	TSharedPtr<PCGExDetails::FDistances> MakeDistances() const { return PCGExDetails::MakeDistances(Source, Target); }
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
};


UENUM()
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

	FVector CWTolerance = FVector::OneVector;
	TSharedPtr<PCGExDetails::FDistances> DistanceDetails;

	/** Check this box if you're fusing over a very large radius and want to ensure determinism. NOTE : Will make things slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Force Determinism"))
	bool bInlineInsertion = true;

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

		DistanceDetails = PCGExDetails::MakeDistances(SourceDistance, TargetDistance);
	}

	bool DoInlineInsertion() const { return bInlineInsertion; }

	FORCEINLINE uint32 GetGridKey(const FVector& Location) const { return PCGEx::GH3(Location + VoxelGridOffset, CWTolerance); }
	FORCEINLINE FBoxCenterAndExtent GetOctreeBox(const FVector& Location) const { return FBoxCenterAndExtent(Location, CWTolerance); }

	FORCEINLINE void GetCenters(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const
	{
		OutSource = DistanceDetails->GetSourceCenter(SourcePoint, SourcePoint.Transform.GetLocation(), TargetPoint.Transform.GetLocation());
		OutTarget = DistanceDetails->GetTargetCenter(TargetPoint, TargetPoint.Transform.GetLocation(), OutSource);
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

#undef PCGEX_SOFT_VALIDATE_NAME_DETAILS
