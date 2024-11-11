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
	IsInside       = 0 UMETA(DisplayName = "Is Inside", Tooltip="..."),
	IsInsideOrOn   = 1 UMETA(DisplayName = "Is Inside or On", Tooltip="..."),
	IsInsideAndOn  = 2 UMETA(DisplayName = "Is Inside and On", Tooltip="..."),
	IsOutside      = 3 UMETA(DisplayName = "Is Outside", Tooltip="..."),
	IsOutsideOrOn  = 4 UMETA(DisplayName = "Is Outside or On", Tooltip="..."),
	IsOutsideAndOn = 5 UMETA(DisplayName = "Is Outside and On", Tooltip="..."),
	IsOn           = 6 UMETA(DisplayName = "Is On", Tooltip="..."),
	IsNotOn        = 7 UMETA(DisplayName = "Is not On", Tooltip="..."),
};

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Fetch Type")--E*/)
enum class EPCGExSplineFilterPick : uint8
{
	Closest = 0 UMETA(DisplayName = "Closest", Tooltip="..."),
	All     = 1 UMETA(DisplayName = "All", Tooltip="...")
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
	EPCGExSplineFilterPick Pick = EPCGExSplineFilterPick::Closest;

	/** Tolerance value used to determine whether a point is considered on the spline or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double Tolerance = 1;

	/** Scale the tolerance with spline' "thickness" (Scale' length)  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bSplineScalesTolerance = false;

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
		Not
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ TSplineFilter final : public PCGExPointFilter::FSimpleFilter
	{
	public:
		explicit TSplineFilter(const TObjectPtr<const UPCGExSplineFilterFactory>& InFactory)
			: FSimpleFilter(InFactory), TypedFilterFactory(InFactory)
		{
			Splines = TypedFilterFactory->Splines;
		}

		const TObjectPtr<const UPCGExSplineFilterFactory> TypedFilterFactory;

		TArray<const FPCGSplineStruct*> Splines;

		double ToleranceSquared = MAX_dbl;
		ESplineCheckFlags CheckFlag = None;
		ESplineMatch Match = Any;

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
