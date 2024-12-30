// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Collections/PCGExMeshCollection.h"
#include "PCGExFitting.h"
#include "PCGExStaging.h"

#include "PCGExAssetStaging.generated.h"

UENUM()
enum class EPCGExStagingOutputMode : uint8
{
	Attributes    = 0 UMETA(DisplayName = "Point Attributes", ToolTip="Write asset data on the point"),
	CollectionMap = 1 UMETA(DisplayName = "Collection Map", ToolTip="Write collection reference & pick for later use"),
};

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
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExStagingOutputMode OutputMode = EPCGExStagingOutputMode::Attributes;

	/** The name of the attribute to write asset path to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="OutputMode==EPCGExStagingOutputMode::Attributes"))
	FName AssetPathAttributeName = "AssetPath";

	/** Distribution details */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Distribution", ShowOnlyInnerProperties))
	FPCGExAssetDistributionDetails DistributionSettings;

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

	TSharedPtr<PCGExStaging::FPickPacker> CollectionPickDatasetPacker;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAssetStagingElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

	PCGEX_CAN_ONLY_EXECUTE_ON_MAIN_THREAD(true)

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

		bool bInherit = false;
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

		TSharedPtr<PCGExData::TBuffer<int64>> HashWriter;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
