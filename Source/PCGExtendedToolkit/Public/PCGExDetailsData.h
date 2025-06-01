// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDataMath.h"
#include "PCGExDetails.h"
#include "PCGExMacros.h"
#include "Data/PCGExData.h"

#include "PCGExDetailsData.generated.h"

#define PCGEX_SETTING_VALUE_GET(_NAME, _TYPE, _INPUT, _SOURCE, _CONSTANT)\
TSharedPtr<PCGExDetails::TSettingValue<_TYPE>> GetValueSetting##_NAME(const bool bQuietErrors = false) const{\
TSharedPtr<PCGExDetails::TSettingValue<_TYPE>> V = PCGExDetails::MakeSettingValue<_TYPE>(_INPUT, _SOURCE, _CONSTANT);\
V->bQuietErrors = bQuietErrors;	return V; }
#define PCGEX_SETTING_VALUE_GET_BOOL(_NAME, _TYPE, _INPUT, _SOURCE, _CONSTANT) PCGEX_SETTING_VALUE_GET(_NAME, _TYPE, _INPUT ? EPCGExInputValueType::Attribute : EPCGExInputValueType::Constant, _SOURCE, _CONSTANT);

namespace PCGExDetails
{
#pragma region Settings

	template <typename T>
	class TSettingValue : public TSharedFromThis<TSettingValue<T>>
	{
	public:
		bool bQuietErrors = false;
		virtual ~TSettingValue() = default;
		virtual bool Init(const FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped = true, const bool bCaptureMinMax = false) = 0;
		FORCEINLINE virtual void SetConstant(T InConstant)
		{
		}

		FORCEINLINE virtual bool IsConstant() { return false; }
		FORCEINLINE virtual T Read(const int32 Index) = 0;
		FORCEINLINE virtual T Min() = 0;
		FORCEINLINE virtual T Max() = 0;
	};

	template <typename T>
	class TSettingValueBuffer final : public TSettingValue<T>
	{
	protected:
		TSharedPtr<PCGExData::TBuffer<T>> Buffer = nullptr;
		FName Name = NAME_None;

	public:
		explicit TSettingValueBuffer(const FName InName):
			Name(InName)
		{
		}

		virtual bool Init(const FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped = true, const bool bCaptureMinMax = false) override
		{
			PCGEX_VALIDATE_NAME_C(InContext, Name)

			if (bSupportScoped && InDataFacade->bSupportsScopedGet) { Buffer = InDataFacade->GetScopedReadable<T>(Name); }
			else { Buffer = InDataFacade->GetReadable<T>(Name); }

			if (!Buffer)
			{
				if (!this->bQuietErrors) { PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Attribute \"{0}\" is missing."), FText::FromName(Name))); }
				return false;
			}

			return true;
		}

		FORCEINLINE virtual T Read(const int32 Index) override { return Buffer->Read(Index); }
		FORCEINLINE virtual T Min() override { return Buffer->Min; }
		FORCEINLINE virtual T Max() override { return Buffer->Max; }
	};

	template <typename T>
	class TSettingValueSelector final : public TSettingValue<T>
	{
	protected:
		TSharedPtr<PCGExData::TBuffer<T>> Buffer = nullptr;
		FPCGAttributePropertyInputSelector Selector;

	public:
		explicit TSettingValueSelector(const FPCGAttributePropertyInputSelector& InSelector):
			Selector(InSelector)
		{
		}

		virtual bool Init(const FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped = true, const bool bCaptureMinMax = false) override
		{
			if (bSupportScoped && InDataFacade->bSupportsScopedGet && !bCaptureMinMax) { Buffer = InDataFacade->GetScopedBroadcaster<T>(Selector); }
			else { Buffer = InDataFacade->GetBroadcaster<T>(Selector, bCaptureMinMax); }

			if (!Buffer)
			{
				if (!this->bQuietErrors) { PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Selector \"{0}\" is invalid."), FText::FromString(PCGEx::GetSelectorDisplayName(Selector)))); }
				return false;
			}


			return true;
		}

		FORCEINLINE virtual T Read(const int32 Index) override { return Buffer->Read(Index); }
		FORCEINLINE virtual T Min() override { return Buffer->Min; }
		FORCEINLINE virtual T Max() override { return Buffer->Max; }
	};

	template <typename T>
	class TSettingValueConstant final : public TSettingValue<T>
	{
	protected:
		T Constant = T{};

	public:
		explicit TSettingValueConstant(const T InConstant):
			Constant(InConstant)
		{
		}

		virtual bool Init(const FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped = true, const bool bCaptureMinMax = false) override { return true; }

		FORCEINLINE virtual bool IsConstant() override { return true; }
		FORCEINLINE virtual void SetConstant(T InConstant) override { Constant = InConstant; };

		FORCEINLINE virtual T Read(const int32 Index) override { return Constant; }
		FORCEINLINE virtual T Min() override { return Constant; }
		FORCEINLINE virtual T Max() override { return Constant; }
	};

	template <typename T>
	static TSharedPtr<TSettingValue<T>> MakeSettingValue(const T InConstant)
	{
		TSharedPtr<TSettingValueConstant<T>> V = MakeShared<TSettingValueConstant<T>>(InConstant);
		return StaticCastSharedPtr<TSettingValue<T>>(V);
	}

	template <typename T>
	static TSharedPtr<TSettingValue<T>> MakeSettingValue(const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const T InConstant)
	{
		if (InInput == EPCGExInputValueType::Attribute)
		{
			TSharedPtr<TSettingValueSelector<T>> V = MakeShared<TSettingValueSelector<T>>(InSelector);
			return StaticCastSharedPtr<TSettingValue<T>>(V);
		}

		return MakeSettingValue<T>(InConstant);
	}

	template <typename T>
	static TSharedPtr<TSettingValue<T>> MakeSettingValue(const EPCGExInputValueType InInput, const FName InName, const T InConstant)
	{
		if (InInput == EPCGExInputValueType::Attribute)
		{
			TSharedPtr<TSettingValueBuffer<T>> V = MakeShared<TSettingValueBuffer<T>>(InName);
			return StaticCastSharedPtr<TSettingValue<T>>(V);
		}

		return MakeSettingValue<T>(InConstant);
	}

#pragma endregion

#pragma region Distances

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

		virtual FVector GetSourceCenter(const PCGExData::FConstPoint& OriginPoint, const FVector& OriginLocation, const FVector& ToCenter) const = 0;
		virtual FVector GetTargetCenter(const PCGExData::FConstPoint& OriginPoint, const FVector& OriginLocation, const FVector& ToCenter) const = 0;
		virtual void GetCenters(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const = 0;
		
		virtual double GetDistSquared(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint) const = 0;
		virtual double GetDistSquared(const PCGExData::FProxyPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint) const = 0;
		virtual double GetDist(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint) const = 0;
		
		virtual double GetDistSquared(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint, bool& bOverlap) const = 0;
		virtual double GetDistSquared(const PCGExData::FProxyPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint, bool& bOverlap) const = 0;
		virtual double GetDist(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint, bool& bOverlap) const = 0;
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

		FORCEINLINE virtual FVector GetSourceCenter(const PCGExData::FConstPoint& FromPoint, const FVector& FromCenter, const FVector& ToCenter) const override
		{
			return PCGExMath::GetSpatializedCenter<Source>(FromPoint, FromCenter, ToCenter);
		}

		FORCEINLINE virtual FVector GetTargetCenter(const PCGExData::FConstPoint& FromPoint, const FVector& FromCenter, const FVector& ToCenter) const override
		{
			return PCGExMath::GetSpatializedCenter<Target>(FromPoint, FromCenter, ToCenter);
		}

		FORCEINLINE virtual void GetCenters(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const override
		{
			const FVector TargetOrigin = TargetPoint.GetLocation();
			OutSource = PCGExMath::GetSpatializedCenter<Source>(SourcePoint, SourcePoint.GetLocation(), TargetOrigin);
			OutTarget = PCGExMath::GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource);
		}

		FORCEINLINE virtual double GetDistSquared(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint) const override
		{
			const FVector TargetOrigin = TargetPoint.GetLocation();
			const FVector OutSource = PCGExMath::GetSpatializedCenter<Source>(SourcePoint, SourcePoint.GetLocation(), TargetOrigin);
			return FVector::DistSquared(OutSource, PCGExMath::GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource));
		}

		FORCEINLINE virtual double GetDistSquared(const PCGExData::FProxyPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint) const override
		{
			const FVector TargetOrigin = TargetPoint.GetLocation();
			const FVector OutSource = PCGExMath::GetSpatializedCenter<Source>(SourcePoint, SourcePoint.GetLocation(), TargetOrigin);
			return FVector::DistSquared(OutSource, PCGExMath::GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource));
		}

		FORCEINLINE virtual double GetDist(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint) const override
		{
			const FVector TargetOrigin = TargetPoint.GetLocation();
			const FVector OutSource = PCGExMath::GetSpatializedCenter<Source>(SourcePoint, SourcePoint.GetLocation(), TargetOrigin);
			return FVector::Dist(OutSource, PCGExMath::GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource));
		}

		FORCEINLINE virtual double GetDistSquared(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint, bool& bOverlap) const override
		{
			const FVector TargetOrigin = TargetPoint.GetLocation();
			const FVector SourceOrigin = SourcePoint.GetLocation();
			const FVector OutSource = PCGExMath::GetSpatializedCenter<Source>(SourcePoint, SourceOrigin, TargetOrigin);
			const FVector OutTarget = PCGExMath::GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource);

			bOverlap = FVector::DotProduct((TargetOrigin - SourceOrigin), (OutTarget - OutSource)) < 0;
			return FVector::DistSquared(OutSource, OutTarget);
		}

		FORCEINLINE virtual double GetDistSquared(const PCGExData::FProxyPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint, bool& bOverlap) const override
		{
			const FVector TargetOrigin = TargetPoint.GetLocation();
			const FVector SourceOrigin = SourcePoint.GetLocation();
			const FVector OutSource = PCGExMath::GetSpatializedCenter<Source>(SourcePoint, SourceOrigin, TargetOrigin);
			const FVector OutTarget = PCGExMath::GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource);

			bOverlap = FVector::DotProduct((TargetOrigin - SourceOrigin), (OutTarget - OutSource)) < 0;
			return FVector::DistSquared(OutSource, OutTarget);
		}

		FORCEINLINE virtual double GetDist(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint, bool& bOverlap) const override
		{
			const FVector TargetOrigin = TargetPoint.GetLocation();
			const FVector SourceOrigin = SourcePoint.GetLocation();
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

#pragma endregion
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
struct PCGEXTENDEDTOOLKIT_API FPCGExInfluenceDetails
{
	GENERATED_BODY()

	FPCGExInfluenceDetails()
	{
	}

	/** Type of Weight */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType InfluenceInput = EPCGExInputValueType::Constant;

	/** Fetch the size from a local attribute. The regular Size parameter then act as a scale.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Influence (Attr)", EditCondition="InfluenceInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector LocalInfluence;

	/** Draw size. What it means depends on the selected debug type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Influence", EditCondition="InfluenceInput==EPCGExInputValueType::Constant", EditConditionHides, ClampMin=-1, ClampMax=1))
	double Influence = 1.0;

	PCGEX_SETTING_VALUE_GET(Influence, double, InfluenceInput, LocalInfluence, Influence)

	/** If enabled, applies influence after each iteration; otherwise applies once at the end of the relaxing.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bProgressiveInfluence = true;

	TSharedPtr<PCGExDetails::TSettingValue<double>> InfluenceBuffer;

	bool Init(const FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InPointDataFacade);
	FORCEINLINE double GetInfluence(const int32 PointIndex) const { return InfluenceBuffer->Read(PointIndex); }
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExComponentTaggingDetails
{
	GENERATED_BODY()

	FPCGExComponentTaggingDetails()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bForwardInputDataTags = true;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	//bool bOutputTagsToAttributes = false;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	//bool bAddTagsToData = false;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExFuseDetailsBase
{
	GENERATED_BODY()

	FPCGExFuseDetailsBase()
	{
		ToleranceInput = EPCGExInputValueType::Constant;
	}

	explicit FPCGExFuseDetailsBase(const bool InSupportLocalTolerance)
		: bSupportLocalTolerance(InSupportLocalTolerance)
	{
		if (!bSupportLocalTolerance) { ToleranceInput = EPCGExInputValueType::Constant; }
	}

	FPCGExFuseDetailsBase(const bool InSupportLocalTolerance, const double InTolerance)
		: bSupportLocalTolerance(InSupportLocalTolerance), Tolerance(InTolerance)
	{
		if (!bSupportLocalTolerance) { ToleranceInput = EPCGExInputValueType::Constant; }
	}

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


	// TODO : Some optimization could be welcome by expos

	FORCEINLINE bool IsWithinTolerance(const double DistSquared, const int32 PointIndex) const
	{
		return FMath::IsWithin<double, double>(DistSquared, 0, FMath::Square(ToleranceGetter->Read(PointIndex).X));
	}

	FORCEINLINE bool IsWithinTolerance(const FVector& Source, const FVector& Target, const int32 PointIndex) const
	{
		return FMath::IsWithin<double, double>(FVector::DistSquared(Source, Target), 0, FMath::Square(ToleranceGetter->Read(PointIndex).X));
	}

	FORCEINLINE bool IsWithinToleranceComponentWise(const FVector& Source, const FVector& Target, const int32 PointIndex) const
	{
		const FVector CWTolerance = ToleranceGetter->Read(PointIndex);
		return
			FMath::IsWithin<double, double>(abs(Source.X - Target.X), 0, CWTolerance.X) &&
			FMath::IsWithin<double, double>(abs(Source.Y - Target.Y), 0, CWTolerance.Y) &&
			FMath::IsWithin<double, double>(abs(Source.Z - Target.Z), 0, CWTolerance.Z);
	}

protected:
	TSharedPtr<PCGExDetails::TSettingValue<FVector>> ToleranceGetter;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSourceFuseDetails : public FPCGExFuseDetailsBase
{
	GENERATED_BODY()

	FPCGExSourceFuseDetails()
		: FPCGExFuseDetailsBase(false)
	{
	};

	explicit FPCGExSourceFuseDetails(const bool InSupportLocalTolerance) :
		FPCGExFuseDetailsBase(InSupportLocalTolerance)
	{
	}

	FPCGExSourceFuseDetails(const bool InSupportLocalTolerance, const double InTolerance)
		: FPCGExFuseDetailsBase(InSupportLocalTolerance, InTolerance)
	{
	}

	explicit FPCGExSourceFuseDetails(const bool InSupportLocalTolerance, const double InTolerance, const EPCGExDistance SourceMethod)
		: FPCGExFuseDetailsBase(InSupportLocalTolerance, InTolerance), SourceDistance(SourceMethod)
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDistance SourceDistance = EPCGExDistance::Center;
};


UENUM()
enum class EPCGExFuseMethod : uint8
{
	Voxel  = 0 UMETA(DisplayName = "Spatial Hash", Tooltip="Fast but blocky. Creates grid-looking approximation."),
	Octree = 1 UMETA(DisplayName = "Octree", Tooltip="Slow but precise. Respectful of the original topology. Requires stable insertion with large values."),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExFuseDetails : public FPCGExSourceFuseDetails
{
	GENERATED_BODY()

	FPCGExFuseDetails()
		: FPCGExSourceFuseDetails(false)
	{
	}

	FPCGExFuseDetails(const bool InSupportLocalTolerance) :
		FPCGExSourceFuseDetails(InSupportLocalTolerance)
	{
	}

	explicit FPCGExFuseDetails(const bool InSupportLocalTolerance, const double InTolerance)
		: FPCGExSourceFuseDetails(InSupportLocalTolerance, InTolerance)
	{
	}

	explicit FPCGExFuseDetails(const bool InSupportLocalTolerance, const double InTolerance, const EPCGExDistance InSourceMethod)
		: FPCGExSourceFuseDetails(InSupportLocalTolerance, InTolerance, InSourceMethod)
	{
	}

	explicit FPCGExFuseDetails(const bool InSupportLocalTolerance, const double InTolerance, const EPCGExDistance InSourceMethod, const EPCGExDistance InTargetMethod)
		: FPCGExSourceFuseDetails(InSupportLocalTolerance, InTolerance, InSourceMethod), TargetDistance(InTargetMethod)
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDistance TargetDistance = EPCGExDistance::Center;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFuseMethod FuseMethod = EPCGExFuseMethod::Voxel;

	/** Offset the voxelized grid by an amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="FuseMethod==EPCGExFuseMethod::Voxel", EditConditionHides))
	FVector VoxelGridOffset = FVector::ZeroVector;

	TSharedPtr<PCGExDetails::FDistances> DistanceDetails;

	/** Check this box if you're fusing over a very large radius and want to ensure insertion order to avoid snapping to different points. NOTE : Will make things considerably slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Stabilize Insertion Order"))
	bool bInlineInsertion = false;

	virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade) override;

	bool DoInlineInsertion() const { return bInlineInsertion; }

	FORCEINLINE uint32 GetGridKey(const FVector& Location, const int32 PointIndex) const
	{
		const FVector Raw = ToleranceGetter->Read(PointIndex);
		return PCGEx::GH3(Location + VoxelGridOffset, FVector(1 / Raw.X, 1 / Raw.Y, 1 / Raw.Z));
	}

	FORCEINLINE FBoxCenterAndExtent GetOctreeBox(const FVector& Location, const int32 PointIndex) const
	{
		return FBoxCenterAndExtent(Location, ToleranceGetter->Read(PointIndex));
	}

	FORCEINLINE void GetCenters(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const
	{
		const FVector TargetLocation = TargetPoint.GetTransform().GetLocation();
		OutSource = DistanceDetails->GetSourceCenter(SourcePoint, SourcePoint.GetTransform().GetLocation(), TargetLocation);
		OutTarget = DistanceDetails->GetTargetCenter(TargetPoint, TargetLocation, OutSource);
	}

	FORCEINLINE bool IsWithinTolerance(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint) const
	{
		FVector A;
		FVector B;
		GetCenters(SourcePoint, TargetPoint, A, B);
		return FPCGExFuseDetailsBase::IsWithinTolerance(A, B, SourcePoint.Index);
	}

	FORCEINLINE bool IsWithinToleranceComponentWise(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint) const
	{
		FVector A;
		FVector B;
		GetCenters(SourcePoint, TargetPoint, A, B);
		return FPCGExFuseDetailsBase::IsWithinToleranceComponentWise(A, B, SourcePoint.Index);
	}
};
