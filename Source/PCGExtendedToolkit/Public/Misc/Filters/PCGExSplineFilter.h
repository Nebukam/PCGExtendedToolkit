// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGSplineData.h"
#include "Sampling/PCGExSampleNearestSpline.h"


#include "PCGExSplineFilter.generated.h"

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Fetch Type")--E*/)
enum class EPCGExSplineCheckType : uint8
{
	IsInside      = 0 UMETA(DisplayName = "Is Inside", Tooltip="..."),
	IsInsideOrOn  = 1 UMETA(DisplayName = "Is Inside or On", Tooltip="..."),
	IsOutside     = 2 UMETA(DisplayName = "Is Outside", Tooltip="..."),
	IsOutsideOrOn = 3 UMETA(DisplayName = "Is Outside or On", Tooltip="..."),
	IsOn          = 4 UMETA(DisplayName = "Is On", Tooltip="..."),
};

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Fetch Type")--E*/)
enum class EPCGExInsideOutFavor : uint8
{
	Closest = 0 UMETA(DisplayName = "Closest", Tooltip="..."),
	Any  = 1 UMETA(DisplayName = "Inside", Tooltip="...")
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSplineFilterConfig
{
	GENERATED_BODY()

	FPCGExSplineFilterConfig()
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
	EPCGExInsideOutFavor Favor = EPCGExInsideOutFavor::Closest;

	/** Tolerance value used to determine whether a point is considered on the spline or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double Tolerance = 1;

	/** Scale the tolerance with spline' "thickness" (Scale' length)  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bSplineScaleTolerance = false;

	/** If enabled, invert the result of the test */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSplineFilterFactory : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExSplineFilterConfig Config;
	TArray<const FPCGSplineStruct*> Splines;
	virtual bool Init(FPCGExContext* InContext) override;
	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;

	virtual void BeginDestroy() override;

	virtual void RegisterConsumableAttributes(FPCGExContext* InContext) const override;
};

namespace PCGExPointsFilter
{
	class /*PCGEXTENDEDTOOLKIT_API*/ TSplineFilter final : public PCGExPointFilter::FSimpleFilter
	{
	public:
		explicit TSplineFilter(const TObjectPtr<const UPCGExSplineFilterFactory>& InFactory)
			: FSimpleFilter(InFactory), TypedFilterFactory(InFactory)
		{
			Splines = TypedFilterFactory->Splines;
		}

		const TObjectPtr<const UPCGExSplineFilterFactory> TypedFilterFactory;

		double ToleranceSquared = MAX_dbl;
		TArray<const FPCGSplineStruct*> Splines;

		using SplineCheckCallback = std::function<bool(const FPCGPoint&)>;
		SplineCheckCallback SplineCheck;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;
		virtual bool Test(const int32 PointIndex) const override;

		virtual ~TSplineFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSplineFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		SplineFilterFactory, "Filter : Spline", "Creates a filter definition that checks points against a spline.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

public:
	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExSplineFilterConfig Config;

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
