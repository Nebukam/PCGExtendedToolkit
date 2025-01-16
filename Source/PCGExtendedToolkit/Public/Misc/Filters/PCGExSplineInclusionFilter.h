// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGSplineData.h"
#include "Sampling/PCGExSampleNearestSpline.h"


#include "PCGExSplineInclusionFilter.generated.h"

UENUM()
enum class EPCGExSplineCheckType : uint8
{
	IsInside       = 0 UMETA(DisplayName = "Is Inside", Tooltip="..."),
	IsInsideOrOn   = 1 UMETA(DisplayName = "Is Inside or On", Tooltip="..."),
	IsInsideAndOn  = 2 UMETA(DisplayName = "Is Inside and On", Tooltip="..."),
	IsOutside      = 3 UMETA(DisplayName = "Is Outside", Tooltip="..."),
	IsOutsideOrOn  = 4 UMETA(DisplayName = "Is Outside or On", Tooltip="..."),
	IsOutsideAndOn = 5 UMETA(DisplayName = "Is Outside and On", Tooltip="..."),
	IsOn           = 6 UMETA(DisplayName = "Is On", Tooltip="..."),
	IsNotOn        = 7 UMETA(DisplayName = "Is not On", Tooltip="..."),
};

UENUM()
enum class EPCGExSplineFilterPick : uint8
{
	Closest = 0 UMETA(DisplayName = "Closest", Tooltip="..."),
	All     = 1 UMETA(DisplayName = "All", Tooltip="...")
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSplineInclusionFilterConfig
{
	GENERATED_BODY()

	FPCGExSplineInclusionFilterConfig()
	{
	}

	/** Sample inputs.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSplineSamplingIncludeMode SampleInputs = EPCGExSplineSamplingIncludeMode::All;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExSplineCheckType CheckType = EPCGExSplineCheckType::IsInside;

	/** If a point is both inside and outside a spline (if there are multiple ones), decide what value to favor. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExSplineFilterPick Pick = EPCGExSplineFilterPick::Closest;

	/** Tolerance value used to determine whether a point is considered on the spline or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	double Tolerance = 1;

	/** Scale the tolerance with spline' "thickness" (Scale' length)  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bSplineScalesTolerance = false;

	/**  Min dot product threshold for a point to be considered inside the spline. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=-1, ClampMax=1))
	double CurvatureThreshold = 0.5;

	/** If enabled, invert the result of the test */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSplineInclusionFilterFactory : public UPCGExFilterFactoryData
{
	GENERATED_BODY()

public:
	FPCGExSplineInclusionFilterConfig Config;
	TArray<FPCGSplineStruct> Splines;
	virtual bool Init(FPCGExContext* InContext) override;
	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;

	virtual void BeginDestroy() override;
};

namespace PCGExPointsFilter
{
	enum ESplineCheckFlags : uint8
	{
		None    = 0,
		Inside  = 1 << 0,
		Outside = 1 << 1,
		On      = 1 << 2,
	};

	enum ESplineMatch : uint8
	{
		Any = 0,
		All,
		Skip
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FSplineInclusionFilter final : public PCGExPointFilter::FSimpleFilter
	{
	public:
		explicit FSplineInclusionFilter(const TObjectPtr<const UPCGExSplineInclusionFilterFactory>& InFactory)
			: FSimpleFilter(InFactory), TypedFilterFactory(InFactory)
		{
			Splines = &TypedFilterFactory->Splines;
		}

		const TObjectPtr<const UPCGExSplineInclusionFilterFactory> TypedFilterFactory;

		const TArray<FPCGSplineStruct>* Splines = nullptr;

		double ToleranceSquared = MAX_dbl;
		ESplineCheckFlags GoodFlags = None;
		ESplineCheckFlags BadFlags = None;
		ESplineMatch GoodMatch = Any;

		using SplineCheckCallback = std::function<bool(const FPCGPoint&)>;
		SplineCheckCallback SplineCheck;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;
		virtual bool Test(const int32 PointIndex) const override;

		virtual ~FSplineInclusionFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSplineInclusionFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		SplineInclusionFilterFactory, "Filter : Spline Inclusion", "Creates a filter definition that checks points inclusion against a spline.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

public:
	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExSplineInclusionFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
