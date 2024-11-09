// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Collections/PCGExMeshCollection.h"
#include "PCGExFitting.h"


#include "PCGExAssetStaging.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExAssetStagingSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		AssetStaging, "Asset Staging", "Data staging from PCGEx Asset Collections.",
		FName(TEXT("[ ") + ( CollectionSource == EPCGExCollectionSource::Asset ? AssetCollection.GetAssetName() : TEXT("Attribute Set to Collection")) + TEXT(" ]")));
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscAdd; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourcePointFiltersLabel, "Filters which points get staged.", PCGExFactories::PointFilters, false)
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExCollectionSource CollectionSource = EPCGExCollectionSource::Asset;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CollectionSource == EPCGExCollectionSource::Asset", EditConditionHides))
	TSoftObjectPtr<UPCGExAssetCollection> AssetCollection;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CollectionSource == EPCGExCollectionSource::AttributeSet", EditConditionHides))
	FPCGExRoamingAssetCollectionDetails AttributeSetDetails;

	/** Distribution details */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Distribution", meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExAssetDistributionDetails DistributionSettings;

	/** The name of the attribute to write asset path to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Distribution", meta=(PCG_Overridable))
	FName AssetPathAttributeName = "AssetPath";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExScaleToFitDetails ScaleToFit;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExJustificationDetails Justification;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	FPCGExFittingVariationsDetails Variations;

	//** If enabled, filter output based on whether a staging has been applied or not (empty entry).  Current implementation is slow. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bPruneEmptyPoints = true;

	/** Tagging details */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable))
	FPCGExAssetTaggingDetails TaggingDetails;

	/** Update point scale so staged asset fits within its bounds */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable))
	EPCGExWeightOutputMode WeightToAttribute = EPCGExWeightOutputMode::NoOutput;

	/** The name of the attribute to write asset weight to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, EditCondition="WeightToAttribute!=EPCGExWeightOutputMode::NoOutput && WeightToAttribute!=EPCGExWeightOutputMode::NormalizedToDensity && WeightToAttribute!=EPCGExWeightOutputMode::NormalizedInvertedToDensity"))
	FName WeightAttributeName = "AssetWeight";
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAssetStagingContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExAssetStagingElement;

	virtual void RegisterAssetDependencies() override;

	TObjectPtr<UPCGExAssetCollection> MainCollection;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAssetStagingElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual void PostLoadAssetsDependencies(FPCGExContext* InContext) const override;
	virtual bool PostBoot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExAssetStaging
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExAssetStagingContext, UPCGExAssetStagingSettings>
	{
		int32 NumPoints = 0;

		bool bOutputWeight = false;
		bool bOneMinusWeight = false;
		bool bNormalizedWeight = false;

		FPCGExFittingDetailsHandler FittingHandler;
		FPCGExFittingVariationsDetails Variations;

		TUniquePtr<PCGExAssetCollection::TDistributionHelper<UPCGExAssetCollection, FPCGExAssetCollectionEntry>> Helper;

		TSharedPtr<PCGExData::TBuffer<int32>> WeightWriter;
		TSharedPtr<PCGExData::TBuffer<double>> NormalizedWeightWriter;

#if PCGEX_ENGINE_VERSION > 503
		TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> PathWriter;
#else
		TSharedPtr<PCGExData::TBuffer<FString>> PathWriter;
#endif

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
