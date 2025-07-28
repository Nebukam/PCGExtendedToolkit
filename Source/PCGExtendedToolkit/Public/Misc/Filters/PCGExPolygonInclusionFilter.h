// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"


#include "PCGExPolygonInclusionFilter.generated.h"

USTRUCT(BlueprintType)
struct FPCGExPolygonInclusionFilterConfig
{
	GENERATED_BODY()

	FPCGExPolygonInclusionFilterConfig()
	{
	}

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseMinInclusionCount = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseMinInclusionCount"))
	int32 MinInclusionCount = 2;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseMaxInclusionCount = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseMaxInclusionCount"))
	int32 MaxInclusionCount = 10;

	/** When defines the resolution of the polygon created from spline data. Lower values means higher fidelity, but slower execution. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName="Spline fidelity", ClampMin=1), AdvancedDisplay)
	double Fidelity = 50;

	/** If enabled, invert the result of the test */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;

	/** If enabled, when used with a collection filter, will use collection bounds as a proxy point instead of per-point testing */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bCheckAgainstDataBounds = false;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class UPCGExPolygonInclusionFilterFactory : public UPCGExFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExPolygonInclusionFilterConfig Config;

	virtual bool SupportsCollectionEvaluation() const override { return Config.bCheckAgainstDataBounds; }
	virtual bool SupportsProxyEvaluation() const override { return true; } // TODO Change this one we support per-point tolerance from attribute

	TSharedPtr<TArray<TSharedPtr<TArray<FVector2D>>>> Polygons;
	TSharedPtr<PCGEx::FIndexedItemOctree> Octree;

	virtual bool Init(FPCGExContext* InContext) override;
	virtual bool WantsPreparation(FPCGExContext* InContext) override;
	virtual PCGExFactories::EPreparationResult Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;

	virtual TSharedPtr<PCGExPointFilter::IFilter> CreateFilter() const override;

	virtual void BeginDestroy() override;
};

namespace PCGExPointFilter
{
	class FPolygonInclusionFilter final : public ISimpleFilter
	{
	public:
		explicit FPolygonInclusionFilter(const TObjectPtr<const UPCGExPolygonInclusionFilterFactory>& InFactory)
			: ISimpleFilter(InFactory), TypedFilterFactory(InFactory)
		{
			Polygons = TypedFilterFactory->Polygons;
			Octree = TypedFilterFactory->Octree;
		}

		const TObjectPtr<const UPCGExPolygonInclusionFilterFactory> TypedFilterFactory;

		TSharedPtr<TArray<TSharedPtr<TArray<FVector2D>>>> Polygons;
		TSharedPtr<PCGEx::FIndexedItemOctree> Octree;

		TConstPCGValueRange<FTransform> InTransforms;
		bool bCheckAgainstDataBounds = false;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;
		virtual bool Test(const PCGExData::FProxyPoint& Point) const override;
		virtual bool Test(const int32 PointIndex) const override;
		virtual bool Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const override;

		virtual ~FPolygonInclusionFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter", meta=(PCGExNodeLibraryDoc="filters/filters-points/spatial/polygon-2d-inclusion"))
class UPCGExPolygonInclusionFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		PolygonInclusionFilterFactory, "Filter : Polygon 2D Inclusion", "Creates a filter definition that checks points inclusion inside polygon. This is resolved on a flat XY plane.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

public:
	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExPolygonInclusionFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
