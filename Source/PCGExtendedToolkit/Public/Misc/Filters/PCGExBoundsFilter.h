// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"


#include "PCGExBoundsFilter.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Fetch Type"))
enum class EPCGExBoundsCheckType : uint8
{
	Overlap = 0 UMETA(DisplayName = "Overlap", Tooltip="Check if the tested point' bounds overlap with the provided bounds."),
	Inside  = 1 UMETA(DisplayName = "Inside", Tooltip="Check if the tested point' bounds are inside the provided bounds."),
	Outside = 2 UMETA(DisplayName = "Outside", Tooltip="Pass if the tested point' bounds are outside the provided bounds."),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBoundsFilterConfig
{
	GENERATED_BODY()

	FPCGExBoundsFilterConfig()
	{
	}

	/** Bounds type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledBounds;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExBoundsCheckType CheckType = EPCGExBoundsCheckType::Overlap;

	/** Epsilon value used to expand the box when testing if IsInside. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double Epsilon = 1e-4;
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
			Cloud = TypedFilterFactory->BoundsDataFacade ? TypedFilterFactory->BoundsDataFacade->GetCloud(TypedFilterFactory->Config.BoundsSource, TypedFilterFactory->Config.Epsilon) : nullptr;
		}

		const TObjectPtr<const UPCGExBoundsFilterFactory> TypedFilterFactory;

		TSharedPtr<PCGExGeo::FPointBoxCloud> Cloud;

		using BoundCheckCallback = std::function<bool(const FPCGPoint&)>;
		BoundCheckCallback BoundCheck;

		virtual bool Init(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;
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
