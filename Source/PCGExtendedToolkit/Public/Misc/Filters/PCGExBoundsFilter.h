// Copyright 2025 Timothé Lapetite and contributors
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

	/** If enabled, a collection will never be tested against itself */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bIgnoreSelf = false;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExBoundsFilterFactory : public UPCGExFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExBoundsFilterConfig Config;
	
	TArray<TSharedPtr<PCGExData::FFacade>> BoundsDataFacades;
	TArray<TSharedPtr<PCGExGeo::FPointBoxCloud>> Clouds;

	virtual bool SupportsLiveTesting() override { return true; }
	
	virtual bool Init(FPCGExContext* InContext) override;
	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;
	
	virtual bool GetRequiresPreparation(FPCGExContext* InContext) override { return true; }
	virtual bool Prepare(FPCGExContext* InContext) override;

	virtual void BeginDestroy() override;
};

namespace PCGExPointsFilter
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FBoundsFilter final : public PCGExPointFilter::FSimpleFilter
	{
	public:
		explicit FBoundsFilter(const TObjectPtr<const UPCGExBoundsFilterFactory>& InFactory)
			: FSimpleFilter(InFactory), TypedFilterFactory(InFactory)
		{
			Clouds = &TypedFilterFactory->Clouds;
			bIgnoreSelf = TypedFilterFactory->Config.bIgnoreSelf;
		}

		const TObjectPtr<const UPCGExBoundsFilterFactory> TypedFilterFactory;
		const TArray<TSharedPtr<PCGExGeo::FPointBoxCloud>>* Clouds = nullptr;

		EPCGExPointBoundsSource BoundsTarget = EPCGExPointBoundsSource::ScaledBounds;
		bool bIgnoreSelf = false;

		using BoundCheckCallback = std::function<bool(const FPCGPoint&)>;
		BoundCheckCallback BoundCheck;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;
		FORCEINLINE virtual bool Test(const FPCGPoint& Point) const override { return BoundCheck(Point); }
		FORCEINLINE virtual bool Test(const int32 PointIndex) const override { return BoundCheck(PointDataFacade->Source->GetInPoint(PointIndex)); }

		virtual ~FBoundsFilter() override
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

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
