﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Collections/PCGExMeshCollection.h"
#include "Transform/PCGExFitting.h"
#include "PCGExStaging.h"
#include "Data/PCGExPointFilter.h"

#include "PCGExAssetStaging.generated.h"

namespace PCGExMT
{
	template <typename T>
	class TScopedNumericValue;
}

UENUM()
enum class EPCGExStagingOutputMode : uint8
{
	Attributes    = 0 UMETA(DisplayName = "Point Attributes", ToolTip="Write asset data on the point"),
	CollectionMap = 1 UMETA(DisplayName = "Collection Map", ToolTip="Write collection reference & pick for later use"),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(Keywords = "stage prepare spawn proxy", PCGExNodeLibraryDoc="assets-management/asset-staging"))
class UPCGExAssetStagingSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		AssetStaging, "Asset Staging", "Data staging from PCGEx Asset Collections.",
		FName(TEXT("[ ") + ( CollectionSource == EPCGExCollectionSource::Asset ? AssetCollection.GetAssetName() : TEXT("Attribute Set to Collection")) + TEXT(" ]")));
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->ColorMiscAdd); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourcePointFiltersLabel, "Filters which points get staged.", PCGExFactories::PointFilters, false)
	//~End UPCGSettings

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExCollectionSource CollectionSource = EPCGExCollectionSource::Asset;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CollectionSource == EPCGExCollectionSource::Asset", EditConditionHides))
	TSoftObjectPtr<UPCGExAssetCollection> AssetCollection;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CollectionSource == EPCGExCollectionSource::AttributeSet", EditConditionHides))
	FPCGExRoamingAssetCollectionDetails AttributeSetDetails;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExStagingOutputMode OutputMode = EPCGExStagingOutputMode::Attributes;

	/** The name of the attribute to write asset path to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="OutputMode == EPCGExStagingOutputMode::Attributes"))
	FName AssetPathAttributeName = "AssetPath";

	/** Distribution details */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Distribution"))
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, EditCondition="WeightToAttribute != EPCGExWeightOutputMode::NoOutput && WeightToAttribute != EPCGExWeightOutputMode::NormalizedToDensity && WeightToAttribute != EPCGExWeightOutputMode::NormalizedInvertedToDensity"))
	FName WeightAttributeName = "AssetWeight";

	//** If enabled, will output mesh material picks. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, EditCondition="OutputMode != EPCGExStagingOutputMode::CollectionMap"))
	bool bOutputMaterialPicks = false;

	//** If > 0 will create dummy attributes for missing material indices up to a maximum; in order to create a full, fixed-length list of valid (yet null) attributes for the static mesh spawner material overrides. Otherwise, will only create attribute for valid indices. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, DisplayName=" ├─ Fixed Max Index", EditCondition="bOutputMaterialPicks && OutputMode != EPCGExStagingOutputMode::CollectionMap", ClampMin="0"))
	int32 MaxMaterialPicks = 0;

	/** Prefix to be used for material slot picks.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, DisplayName=" └─ Prefix", EditCondition="bOutputMaterialPicks && OutputMode != EPCGExStagingOutputMode::CollectionMap"))
	FName MaterialAttributePrefix = "Mat";

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bDoOutputSockets = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_NotOverridable, DisplayName="Output Sockets", EditCondition="bDoOutputSockets"))
	FPCGExSocketOutputDetails OutputSocketDetails;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors")
	bool bQuietEmptyCollectionError = false;
};

struct FPCGExAssetStagingContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExAssetStagingElement;

	virtual void RegisterAssetDependencies() override;

	TObjectPtr<UPCGExAssetCollection> MainCollection;
	bool bPickMaterials = false;

	TSharedPtr<PCGExStaging::FPickPacker> CollectionPickDatasetPacker;

	FPCGExSocketOutputDetails OutputSocketDetails;
	TSharedPtr<PCGExData::FPointIOCollection> SocketsCollection;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExAssetStagingElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(AssetStaging)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual void PostLoadAssetsDependencies(FPCGExContext* InContext) const override;
	virtual bool PostBoot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override;
};

namespace PCGExAssetStaging
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExAssetStagingContext, UPCGExAssetStagingSettings>
	{
		int32 NumPoints = 0;
		int32 NumInvalid = 0;

		bool bInherit = false;
		bool bOutputWeight = false;
		bool bOneMinusWeight = false;
		bool bNormalizedWeight = false;
		bool bUsesDensity = false;

		TArray<int8> Mask;

		FPCGExFittingDetailsHandler FittingHandler;
		FPCGExFittingVariationsDetails Variations;

		TSharedPtr<PCGExStaging::TDistributionHelper<UPCGExAssetCollection, FPCGExAssetCollectionEntry>> Helper;
		TSharedPtr<PCGExStaging::FSocketHelper> SocketHelper;

		TSharedPtr<PCGExData::TBuffer<int32>> WeightWriter;
		TSharedPtr<PCGExData::TBuffer<double>> NormalizedWeightWriter;

		TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> PathWriter;

		// Material handling 
		TSharedPtr<PCGExMT::TScopedNumericValue<int8>> HighestSlotIndex;
		TArray<TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>>> MaterialWriters; // Per valid slot writers

		TArray<const FPCGExAssetCollectionEntry*> CachedPicks;
		TArray<int8> MaterialPick;

		TSharedPtr<PCGExData::TBuffer<int64>> HashWriter;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;

		virtual void CompleteWork() override;
		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
		virtual void OnRangeProcessingComplete() override;

		virtual void Write() override;
	};
}
