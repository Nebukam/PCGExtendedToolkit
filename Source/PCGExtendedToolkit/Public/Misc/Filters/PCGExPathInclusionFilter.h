// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"
#include "PCGExSplineInclusionFilter.h"
#include "Paths/PCGExPaths.h"
#include "Sampling/PCGExSampleNearestSpline.h"


#include "PCGExPathInclusionFilter.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathInclusionFilterConfig
{
	GENERATED_BODY()

	FPCGExPathInclusionFilterConfig()
	{
	}

	/** Closed loop handling.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExPathClosedLoopDetails ClosedLoop;

	/** Which point type to use. Shared amongst all points; if you want tight control, create a fully-fledged spline instead. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSplinePointTypeRedux PointType = EPCGExSplinePointTypeRedux::Linear;

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
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPathInclusionFilterFactory : public UPCGExFilterFactoryData
{
	GENERATED_BODY()

public:
	FPCGExPathInclusionFilterConfig Config;
	TArray<TSharedPtr<const FPCGSplineStruct>> Splines;
	virtual bool Init(FPCGExContext* InContext) override;
	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;

	virtual void BeginDestroy() override;
};

namespace PCGExPointsFilter
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FPathInclusionFilter final : public PCGExPointFilter::FSimpleFilter
	{
	public:
		explicit FPathInclusionFilter(const TObjectPtr<const UPCGExPathInclusionFilterFactory>& InFactory)
			: FSimpleFilter(InFactory), TypedFilterFactory(InFactory)
		{
			Splines = TypedFilterFactory->Splines;
		}

		const TObjectPtr<const UPCGExPathInclusionFilterFactory> TypedFilterFactory;

		TArray<TSharedPtr<const FPCGSplineStruct>> Splines;

		double ToleranceSquared = MAX_dbl;
		ESplineCheckFlags GoodFlags = None;
		ESplineCheckFlags BadFlags = None;
		ESplineMatch GoodMatch = Any;

		using SplineCheckCallback = std::function<bool(const FPCGPoint&)>;
		SplineCheckCallback SplineCheck;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;
		virtual bool Test(const int32 PointIndex) const override;

		virtual ~FPathInclusionFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPathInclusionFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		PathInclusionFilterFactory, "Filter : Path Inclusion", "Creates a filter definition that checks points inclusion against a path.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

public:
	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExPathInclusionFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
