// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExMacros.h"
#include "Metadata/PCGAttributePropertySelector.h"

#include "PCGExDetailsData.generated.h"

#define PCGEX_SETTING_VALUE_GET(_NAME, _TYPE, _INPUT, _SOURCE, _CONSTANT)\
TSharedPtr<PCGExDetails::TSettingValue<_TYPE>> GetValueSetting##_NAME(const bool bQuietErrors = false) const{\
TSharedPtr<PCGExDetails::TSettingValue<_TYPE>> V = PCGExDetails::MakeSettingValue<_TYPE>(_INPUT, _SOURCE, _CONSTANT);\
V->bQuietErrors = bQuietErrors;	return V; }
#define PCGEX_SETTING_VALUE_GET_BOOL(_NAME, _TYPE, _INPUT, _SOURCE, _CONSTANT) PCGEX_SETTING_VALUE_GET(_NAME, _TYPE, _INPUT ? EPCGExInputValueType::Attribute : EPCGExInputValueType::Constant, _SOURCE, _CONSTANT);

#define PCGEX_SETTING_DATA_VALUE_GET(_NAME, _TYPE, _INPUT, _SOURCE, _CONSTANT)\
TSharedPtr<PCGExDetails::TSettingValue<_TYPE>> GetDataValueSetting##_NAME(FPCGExContext* InContext, const UPCGData* InData, const bool bQuietErrors = false) const{\
TSharedPtr<PCGExDetails::TSettingValue<_TYPE>> V = PCGExDetails::MakeSettingValue<_TYPE>(InContext, InData, _INPUT, _SOURCE, _CONSTANT);\
V->bQuietErrors = bQuietErrors;	return V; }
#define PCGEX_SETTING_DATA_VALUE_GET_BOOL(_NAME, _TYPE, _INPUT, _SOURCE, _CONSTANT) PCGEX_SETTING_DATA_VALUE_GET(_NAME, _TYPE, _INPUT ? EPCGExInputValueType::Attribute : EPCGExInputValueType::Constant, _SOURCE, _CONSTANT);


struct FPCGExContext;

namespace PCGExData
{
	struct FProxyPoint;
	struct FConstPoint;
	class FFacade;
	class FPointIO;

	template <typename T>
	class TBuffer;
}

UENUM()
enum class EPCGExFuseMethod : uint8
{
	Voxel  = 0 UMETA(DisplayName = "Spatial Hash", Tooltip="Fast but blocky. Creates grid-looking approximation."),
	Octree = 1 UMETA(DisplayName = "Octree", Tooltip="Slow but precise. Respectful of the original topology. Requires stable insertion with large values."),
};

UENUM()
enum class EPCGExManhattanMethod : uint8
{
	Simple       = 0 UMETA(DisplayName = "Simple", ToolTip="Simple Manhattan subdivision, will generate 0..2 points"),
	GridDistance = 1 UMETA(DisplayName = "Grid (Distance)", ToolTip="Grid Manhattan subdivision, will subdivide space according to a grid size."),
	GridCount    = 2 UMETA(DisplayName = "Grid (Count)", ToolTip="Grid Manhattan subdivision, will subdivide space according to a grid size."),
};

UENUM()
enum class EPCGExManhattanAlign : uint8
{
	World    = 0 UMETA(DisplayName = "World", ToolTip=""),
	Custom   = 1 UMETA(DisplayName = "Custom", ToolTip=""),
	SegmentX = 5 UMETA(DisplayName = "Segment X", ToolTip=""),
	SegmentY = 6 UMETA(DisplayName = "Segment Y", ToolTip=""),
	SegmentZ = 7 UMETA(DisplayName = "Segment Z", ToolTip=""),
};

namespace PCGExDetails
{
#pragma region Settings

	template <typename T>
	class TSettingValue : public TSharedFromThis<TSettingValue<T>>
	{
	public:
		bool bQuietErrors = false;
		virtual ~TSettingValue() = default;
		virtual bool Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped = true, const bool bCaptureMinMax = false) = 0;
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

		virtual bool Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped = true, const bool bCaptureMinMax = false) override;

		virtual T Read(const int32 Index) override;
		virtual T Min() override;
		virtual T Max() override;
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

		virtual bool Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped = true, const bool bCaptureMinMax = false) override;

		virtual T Read(const int32 Index) override;
		virtual T Min() override;
		virtual T Max() override;
	};

	template <typename T>
	class TSettingValueConstant : public TSettingValue<T>
	{
	protected:
		T Constant = T{};

	public:
		explicit TSettingValueConstant(const T InConstant):
			Constant(InConstant)
		{
		}

		virtual bool Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped = true, const bool bCaptureMinMax = false) override;

		FORCEINLINE virtual bool IsConstant() override { return true; }
		FORCEINLINE virtual void SetConstant(T InConstant) override { Constant = InConstant; };

		FORCEINLINE virtual T Read(const int32 Index) override { return Constant; }
		FORCEINLINE virtual T Min() override { return Constant; }
		FORCEINLINE virtual T Max() override { return Constant; }
	};

	template <typename T>
	class TSettingValueSelectorConstant final : public TSettingValueConstant<T>
	{
	protected:
		FPCGAttributePropertyInputSelector Selector;

	public:
		explicit TSettingValueSelectorConstant(const FPCGAttributePropertyInputSelector& InSelector):
			TSettingValueConstant<T>(T{}), Selector(InSelector)
		{
		}

		virtual bool Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped = true, const bool bCaptureMinMax = false) override;
	};

	template <typename T>
	class TSettingValueBufferConstant final : public TSettingValueConstant<T>
	{
	protected:
		FName Name = NAME_None;

	public:
		explicit TSettingValueBufferConstant(const FName InName):
			TSettingValueConstant<T>(T{}), Name(InName)
		{
		}

		virtual bool Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped = true, const bool bCaptureMinMax = false) override;
	};

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(const T InConstant);

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const T InConstant);

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(const EPCGExInputValueType InInput, const FName InName, const T InConstant);

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType InInput, const FName InName, const T InConstant);

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const T InConstant);

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(const TSharedPtr<PCGExData::FPointIO> InData, const EPCGExInputValueType InInput, const FName InName, const T InConstant);

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(const TSharedPtr<PCGExData::FPointIO> InData, const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const T InConstant);

#pragma region externalization

#define PCGEX_TPL(_TYPE, _NAME, ...) \
extern template class TSettingValueBuffer<_TYPE>; \
extern template class TSettingValueSelector<_TYPE>; \
extern template class TSettingValueConstant<_TYPE>; \
extern template class TSettingValueSelectorConstant<_TYPE>; \
extern template class TSettingValueBufferConstant<_TYPE>; \
extern template TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(const _TYPE InConstant); \
extern template TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const _TYPE InConstant); \
extern template TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(const EPCGExInputValueType InInput, const FName InName, const _TYPE InConstant); \
extern template TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType InInput, const FName InName, const _TYPE InConstant); \
extern template TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const _TYPE InConstant); \
extern template TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(const TSharedPtr<PCGExData::FPointIO> InData, const EPCGExInputValueType InInput, const FName InName, const _TYPE InConstant); \
extern template TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(const TSharedPtr<PCGExData::FPointIO> InData, const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const _TYPE InConstant);

	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)

#undef PCGEX_TPL

#pragma endregion

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
	class TDistances final : public FDistances
	{
	public:
		TDistances()
		{
		}

		explicit TDistances(const bool InOverlapIsZero)
			: FDistances(InOverlapIsZero)
		{
		}

		virtual FVector GetSourceCenter(const PCGExData::FConstPoint& FromPoint, const FVector& FromCenter, const FVector& ToCenter) const override;		
		virtual FVector GetTargetCenter(const PCGExData::FConstPoint& FromPoint, const FVector& FromCenter, const FVector& ToCenter) const override;		
		virtual void GetCenters(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const override;		
		virtual double GetDistSquared(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint) const override;
		

		virtual double GetDistSquared(const PCGExData::FProxyPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint) const override;		
		virtual double GetDist(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint) const override;		
		virtual double GetDistSquared(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint, bool& bOverlap) const override;		
		virtual double GetDistSquared(const PCGExData::FProxyPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint, bool& bOverlap) const override;
		
		virtual double GetDist(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint, bool& bOverlap) const override;
		
	};

extern template class TDistances<EPCGExDistance::Center, EPCGExDistance::Center>; 
extern template class TDistances<EPCGExDistance::Center, EPCGExDistance::SphereBounds>; 
extern template class TDistances<EPCGExDistance::Center, EPCGExDistance::BoxBounds>; 
extern template class TDistances<EPCGExDistance::Center, EPCGExDistance::None>; 
extern template class TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::Center>; 
extern template class TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::SphereBounds>; 
extern template class TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::BoxBounds>; 
extern template class TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::None>; 
extern template class TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::Center>; 
extern template class TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::SphereBounds>; 
extern template class TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::BoxBounds>; 
extern template class TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::None>; 
extern template class TDistances<EPCGExDistance::None, EPCGExDistance::Center>; 
extern template class TDistances<EPCGExDistance::None, EPCGExDistance::SphereBounds>; 
extern template class TDistances<EPCGExDistance::None, EPCGExDistance::BoxBounds>; 
extern template class TDistances<EPCGExDistance::None, EPCGExDistance::None>; 

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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Influence (Attr)", EditCondition="InfluenceInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector LocalInfluence;

	/** Draw size. What it means depends on the selected debug type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Influence", EditCondition="InfluenceInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=-1, ClampMax=1))
	double Influence = 1.0;

	PCGEX_SETTING_VALUE_GET(Influence, double, InfluenceInput, LocalInfluence, Influence)

	/** If enabled, applies influence after each iteration; otherwise applies once at the end of the relaxing.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bProgressiveInfluence = true;

	TSharedPtr<PCGExDetails::TSettingValue<double>> InfluenceBuffer;

	bool Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InPointDataFacade);
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

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExManhattanDetails
{
	GENERATED_BODY()

	explicit FPCGExManhattanDetails(const bool InSupportAttribute = false)
		: bSupportAttribute(InSupportAttribute)
	{
	}

	UPROPERTY()
	bool bSupportAttribute = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExManhattanMethod Method = EPCGExManhattanMethod::Simple;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExAxisOrder Order = EPCGExAxisOrder::XYZ;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="bSupportAttribute", EditConditionHides))
	EPCGExInputValueType GridSizeInput = EPCGExInputValueType::Constant;

	/** Max Length Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName="Grid Size (Attr)", EditCondition="bSupportAttribute && Method != EPCGExManhattanMethod::Simple && GridSizeInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName GridSizeAttribute = FName("GridSize");

	/** Grid Size Constant -- If using count, values will be rounded down to the nearest int. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Grid Size", EditCondition="Method != EPCGExManhattanMethod::Simple && (!bSupportAttribute || GridSizeInput == EPCGExInputValueType::Constant)", EditConditionHides))
	FVector GridSize = FVector(10);

	PCGEX_SETTING_VALUE_GET(GridSize, FVector, GridSizeInput, GridSizeAttribute, GridSize)

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExManhattanAlign SpaceAlign = EPCGExManhattanAlign::World;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bSupportAttribute && SpaceAlign == EPCGExManhattanAlign::Custom", EditConditionHides))
	EPCGExInputValueType OrientInput = EPCGExInputValueType::Constant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Orient (Attr)", EditCondition="bSupportAttribute && OrientInput != EPCGExInputValueType::Constant && SpaceAlign == EPCGExManhattanAlign::Custom", EditConditionHides))
	FPCGAttributePropertyInputSelector OrientAttribute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Orient", ClampMin=0, EditCondition="SpaceAlign == EPCGExManhattanAlign::Custom && (!bSupportAttribute || OrientInput == EPCGExInputValueType::Constant)", EditConditionHides))
	FQuat OrientConstant = FQuat::Identity;

	PCGEX_SETTING_VALUE_GET(Orient, FQuat, OrientInput, OrientAttribute, OrientConstant)

	bool IsValid() const;
	bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade);
	int32 ComputeSubdivisions(const FVector& A, const FVector& B, const int32 Index, TArray<FVector>& OutSubdivisions, double& OutDist) const;

protected:
	bool bInitialized = false;

	int32 Comps[3] = {0, 0, 0};
	TSharedPtr<PCGExDetails::TSettingValue<FVector>> GridSizeBuffer;
	TSharedPtr<PCGExDetails::TSettingValue<FQuat>> OrientBuffer;
};
