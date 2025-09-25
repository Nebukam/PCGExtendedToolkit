﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"


#include "Geometry/PCGExGeoPointBox.h"


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
struct FPCGExBoundsFilterConfig
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="TestMode == EPCGExBoxCheckMode::ExpandedBox || TestMode == EPCGExBoxCheckMode::ExpandedSphere", EditConditionHides))
	double Expansion = 10;

	/** If enabled, invert the result of the test */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;

	/** If enabled, a collection will never be tested against itself */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bIgnoreSelf = false;

	/** If enabled, when used with a collection filter, will use collection bounds as a proxy point instead of per-point testing */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bCheckAgainstDataBounds = false;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class UPCGExBoundsFilterFactory : public UPCGExPointFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExBoundsFilterConfig Config;

	TArray<TSharedPtr<PCGExData::FFacade>> BoundsDataFacades;
	TArray<TSharedPtr<PCGExGeo::FPointBoxCloud>> Clouds;

	virtual bool SupportsCollectionEvaluation() const override { return Config.bCheckAgainstDataBounds; }
	virtual bool SupportsProxyEvaluation() const override { return true; }

	virtual TSharedPtr<PCGExPointFilter::IFilter> CreateFilter() const override;

	virtual bool WantsPreparation(FPCGExContext* InContext) override { return true; }
	virtual PCGExFactories::EPreparationResult Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;

	virtual void BeginDestroy() override;
};

namespace PCGExPointFilter
{
	class FBoundsFilter final : public ISimpleFilter
	{
	public:
		explicit FBoundsFilter(const TObjectPtr<const UPCGExBoundsFilterFactory>& InFactory)
			: ISimpleFilter(InFactory), TypedFilterFactory(InFactory)
		{
			Clouds = &TypedFilterFactory->Clouds;
			bIgnoreSelf = TypedFilterFactory->Config.bIgnoreSelf;
		}

		const TObjectPtr<const UPCGExBoundsFilterFactory> TypedFilterFactory;
		const TArray<TSharedPtr<PCGExGeo::FPointBoxCloud>>* Clouds = nullptr;

		EPCGExPointBoundsSource BoundsTarget = EPCGExPointBoundsSource::ScaledBounds;
		bool bIgnoreSelf = false;
		bool bCheckAgainstDataBounds = false;

		using BoundCheckProxyCallback = std::function<bool(const PCGExData::FProxyPoint&)>;
		BoundCheckProxyCallback BoundCheckProxy;

		using BoundCheckCallback = std::function<bool(const PCGExData::FConstPoint&)>;
		BoundCheckCallback BoundCheck;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;
		virtual bool Test(const PCGExData::FProxyPoint& Point) const override { return BoundCheckProxy(Point); }
		virtual bool Test(const int32 PointIndex) const override;
		virtual bool Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const override;

		virtual ~FBoundsFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter", meta=(PCGExNodeLibraryDoc="filters/filters-points/spatial/bounds"))
class UPCGExBoundsFilterProviderSettings : public UPCGExFilterProviderSettings
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

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
	virtual bool ShowMissingDataHandling_Internal() const override { return true; }
#endif
};
