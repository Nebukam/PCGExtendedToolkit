// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#define PCGEX_SOFT_VALIDATE_NAME_DETAILS(_BOOL, _NAME, _CTX) if(_BOOL){if (!FPCGMetadataAttributeBase::IsValidName(_NAME) || _NAME.IsNone()){ PCGE_LOG_C(Warning, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); _BOOL = false; } }
#define PCGEX_SETTING_VALUE_GET(_NAME, _TYPE, _INPUT, _SOURCE, _CONSTANT)\
	TSharedPtr<PCGExDetails::TSettingValue<_TYPE>> GetValueSetting##_NAME(const bool bQuietErrors = false) const{\
	TSharedPtr<PCGExDetails::TSettingValue<_TYPE>> V = PCGExDetails::MakeSettingValue<_TYPE>(_INPUT, _SOURCE, _CONSTANT);\
	V->bQuietErrors = bQuietErrors;	return V; }
#define PCGEX_SETTING_VALUE_GET_BOOL(_NAME, _TYPE, _INPUT, _SOURCE, _CONSTANT) PCGEX_SETTING_VALUE_GET(_NAME, _TYPE, _INPUT ? EPCGExInputValueType::Attribute : EPCGExInputValueType::Constant, _SOURCE, _CONSTANT);
#include "CoreMinimal.h"
#include "PCGExMacros.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTag.h"

#include "PCGExDetailsData.generated.h"

namespace PCGExDetails
{
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
}

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

	FORCEINLINE void GetCenters(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const
	{
		OutSource = DistanceDetails->GetSourceCenter(SourcePoint, SourcePoint.Transform.GetLocation(), TargetPoint.Transform.GetLocation());
		OutTarget = DistanceDetails->GetTargetCenter(TargetPoint, TargetPoint.Transform.GetLocation(), OutSource);
	}

	FORCEINLINE bool IsWithinTolerance(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, const int32 PointIndex) const
	{
		FVector A;
		FVector B;
		GetCenters(SourcePoint, TargetPoint, A, B);
		return FPCGExFuseDetailsBase::IsWithinTolerance(A, B, PointIndex);
	}

	FORCEINLINE bool IsWithinToleranceComponentWise(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, const int32 PointIndex) const
	{
		FVector A;
		FVector B;
		GetCenters(SourcePoint, TargetPoint, A, B);
		return FPCGExFuseDetailsBase::IsWithinToleranceComponentWise(A, B, PointIndex);
	}
};

#undef PCGEX_SOFT_VALIDATE_NAME_DETAILS
