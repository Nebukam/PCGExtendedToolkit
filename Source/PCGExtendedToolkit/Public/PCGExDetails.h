// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMacros.h"
#include "PCGEx.h"
#include "PCGExH.h"
#include "PCGExMath.h"

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
	class PCGEXTENDEDTOOLKIT_API FDistances : public TSharedFromThis<FDistances>
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

		virtual FVector GetSourceCenter(const FPCGPoint& OriginPoint, const FVector& OriginLocation, const FVector& ToCenter) const = 0;
		virtual FVector GetTargetCenter(const FPCGPoint& OriginPoint, const FVector& OriginLocation, const FVector& ToCenter) const = 0;
		virtual void GetCenters(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const = 0;
		virtual double GetDistSquared(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint) const = 0;
		virtual double GetDist(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint) const = 0;
		virtual double GetDistSquared(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, bool& bOverlap) const = 0;
		virtual double GetDist(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, bool& bOverlap) const = 0;
	};

	template <EPCGExDistance Source, EPCGExDistance Target>
	class PCGEXTENDEDTOOLKIT_API TDistances final : public FDistances
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

	PCGEXTENDEDTOOLKIT_API
	TSharedPtr<FDistances> MakeDistances(
		const EPCGExDistance Source = EPCGExDistance::Center,
		const EPCGExDistance Target = EPCGExDistance::Center,
		const bool bOverlapIsZero = false);

	PCGEXTENDEDTOOLKIT_API
	TSharedPtr<FDistances> MakeNoneDistances();
}


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExDistanceDetails
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

	TSharedPtr<PCGExDetails::FDistances> MakeDistances() const;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExCollisionDetails
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
	FPCGActorSelectorSettings IgnoredActorSelector;

	TArray<AActor*> IgnoredActors;

	UWorld* World = nullptr;

	void Init(const FPCGExContext* InContext);
	void Update(FCollisionQueryParams& InCollisionParams) const;
	bool Linecast(const FVector& From, const FVector& To, FHitResult& HitResult) const;
};

#undef PCGEX_SOFT_VALIDATE_NAME_DETAILS
