// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"


#include "PCGExBoundsFilter.generated.h"

UENUM()
enum class EPCGExBoundsCheckType : uint8
{
	Intersects           = 0 UMETA(DisplayName = "Intersects", Tooltip="..."),
	IsInside             = 1 UMETA(DisplayName = "Is Inside", Tooltip="..."),
	IsInsideOrOn         = 2 UMETA(DisplayName = "Is Inside or On", Tooltip="..."),
	IsInsideOrIntersects = 3 UMETA(DisplayName = "Is Inside or Intersects", Tooltip="..."),
};

UENUM()
enum class EPCGExBoundsFilterCompareMode : uint8
{
	PerPointBounds   = 0 UMETA(DisplayName = "Per Point Bounds", Tooltip="..."),
	CollectionBounds = 1 UMETA(DisplayName = "Collection Bounds", Tooltip="..."),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBoundsFilterConfig
{
	GENERATED_BODY()

	FPCGExBoundsFilterConfig()
	{
	}

	/** Bounds to use on input points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExBoundsFilterCompareMode Mode = EPCGExBoundsFilterCompareMode::PerPointBounds;

	/** Bounds to use on input points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledBounds;

	/** Bounds to use on input bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointBoundsSource BoundsTarget = EPCGExPointBoundsSource::ScaledBounds;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExBoundsCheckType CheckType = EPCGExBoundsCheckType::Intersects;

	/** Defines against what type of shape (extrapolated from target bounds) is tested against. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExBoxCheckMode TestMode = EPCGExBoxCheckMode::Box;

	/** Epsilon value used to slightly expand target bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="TestMode==EPCGExBoxCheckMode::ExpandedBox || TestMode==EPCGExBoxCheckMode::ExpandedSphere", EditConditionHides))
	double Expansion = 10;

	/** If enabled, invert the result of the test */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExBoundsFilterFactory : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExBoundsFilterConfig Config;
	TSharedPtr<PCGExData::FFacade> BoundsDataFacade;
	virtual bool Init(FPCGExContext* InContext) override;
	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;

	virtual void BeginDestroy() override;
};

namespace PCGExPointsFilter
{
	class /*PCGEXTENDEDTOOLKIT_API*/ TBoundsFilter final : public PCGExPointFilter::FSimpleFilter
	{
	public:
		explicit TBoundsFilter(const TObjectPtr<const UPCGExBoundsFilterFactory>& InFactory)
			: FSimpleFilter(InFactory), TypedFilterFactory(InFactory)
		{
			Cloud = TypedFilterFactory->BoundsDataFacade ? TypedFilterFactory->BoundsDataFacade->GetCloud(
					        TypedFilterFactory->Config.BoundsTarget,
					        TypedFilterFactory->Config.TestMode == EPCGExBoxCheckMode::ExpandedBox || TypedFilterFactory->Config.TestMode == EPCGExBoxCheckMode::ExpandedSphere ?
						        TypedFilterFactory->Config.Expansion * 2 : TypedFilterFactory->Config.Expansion) : nullptr;
		}

		const TObjectPtr<const UPCGExBoundsFilterFactory> TypedFilterFactory;

		TSharedPtr<PCGExGeo::FPointBoxCloud> Cloud;
		EPCGExPointBoundsSource BoundsTarget = EPCGExPointBoundsSource::ScaledBounds;

		using BoundCheckCallback = std::function<bool(const FPCGPoint&)>;
		BoundCheckCallback BoundCheck;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;
		FORCEINLINE virtual bool Test(const int32 PointIndex) const override { return BoundCheck(PointDataFacade->Source->GetInPoint(PointIndex)); }

		virtual ~TBoundsFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExBoundsFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		BoundsFilterFactory, "Filter : Bounds", "Creates a filter definition that compares dot value of two vectors.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

public:
	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExBoundsFilterConfig Config;

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
