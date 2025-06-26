// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExDetailsData.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"


#include "PCGExPathAngleFilter.generated.h"

UENUM()
enum class EPCGExPathAngleFilterMode : uint8
{
	Curvature = 0 UMETA(DisplayName = "Curvature", Tooltip="Check against the dot product of (Prev to Current) -> (Current to Next)"),
	Spread    = 1 UMETA(DisplayName = "Spread", Tooltip="Check against the dot product of (Current to Prev) -> (Current to Next)"),
};

USTRUCT(BlueprintType)
struct FPCGExPathAngleFilterConfig
{
	GENERATED_BODY()

	FPCGExPathAngleFilterConfig()
	{
	}

	/** Filter mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExPathAngleFilterMode Mode = EPCGExPathAngleFilterMode::Curvature;

	/** What should this filter return when dealing with first points? (if the data doesn't have @Data.IsClosed = true, otherwise wraps) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExFilterFallback FirstPointFallback = EPCGExFilterFallback::Fail;

	/** What should this filter return when dealing with last points? (if the data doesn't have @Data.IsClosed = true, otherwise wraps) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExFilterFallback LastPointFallback = EPCGExFilterFallback::Fail;

	/** Dot comparison settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExDotComparisonDetails DotComparisonDetails;

	/** Whether the result of the filter should be inverted or not. Note that this will also invert fallback results! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	bool bInvert = false;

	void Sanitize()
	{
	}
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class UPCGExPathAngleFilterFactory : public UPCGExFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExPathAngleFilterConfig Config;

	virtual bool Init(FPCGExContext* InContext) override;

	virtual bool DomainCheck() override;

	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;
	virtual bool SupportsCollectionEvaluation() const override { return false; }
	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;
};

namespace PCGExPointFilter
{
	class FPathAngleFilter final : public FSimpleFilter
	{
	public:
		explicit FPathAngleFilter(const TObjectPtr<const UPCGExPathAngleFilterFactory>& InFactory)
			: FSimpleFilter(InFactory), TypedFilterFactory(InFactory)
		{
			DotComparison = TypedFilterFactory->Config.DotComparisonDetails;
		}

		const TObjectPtr<const UPCGExPathAngleFilterFactory> TypedFilterFactory;

		bool bIsClosed = false;
		FPCGExDotComparisonDetails DotComparison;
		TConstPCGValueRange<FTransform> InTransforms;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;

		virtual bool Test(const int32 PointIndex) const override;

		virtual ~FPathAngleFilter() override
		{
		}
	};
}

///

UCLASS(Hidden, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter", meta=(PCGExNodeLibraryDoc="filters/filters-points/math-checks/dot-product"))
class UPCGExPathAngleFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		PathAngleFilterFactory, "Filter : Path Angle", "Creates a filter definition that compares dot value of the direction of a point toward its previous and next points.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExPathAngleFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
