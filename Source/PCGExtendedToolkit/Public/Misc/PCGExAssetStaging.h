// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "AssetSelectors/PCGExMeshCollection.h"
#include "PCGExAssetStaging.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Distribution"))
enum class EPCGExDistribution : uint8
{
	Index UMETA(DisplayName = "Index", ToolTip="Distribution by index"),
	Random UMETA(DisplayName = "Random", ToolTip="Update the point scale so final asset matches the existing point' bounds"),
	WeightedRandom UMETA(DisplayName = "Weighted random", ToolTip="Update the point bounds so it reflects the bounds of the final asset"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Distribution"))
enum class EPCGExWeightOutputMode : uint8
{
	NoOutput UMETA(DisplayName = "No Output", ToolTip="Don't output weight as an attribute"),
	Raw UMETA(DisplayName = "Raw", ToolTip="Raw integer"),
	Normalized UMETA(DisplayName = "Normalized", ToolTip="Normalized weight value (Weight / WeightSum)"),
	NormalizedInverted UMETA(DisplayName = "Normalized (Inverted)", ToolTip="One Minus normalized weight value (1 - (Weight / WeightSum))"),

	NormalizedToDensity UMETA(DisplayName = "Normalized to Density", ToolTip="Normalized weight value (Weight / WeightSum)"),
	NormalizedInvertedToDensity UMETA(DisplayName = "Normalized (Inverted) to Density", ToolTip="One Minus normalized weight value (1 - (Weight / WeightSum))"),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExAssetStagingSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

	UPCGExAssetStagingSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		if (IndexSource.GetName() == FName("@Last")) { IndexSource.Update(TEXT("$Index")); }
	}

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		AssetStaging, "Asset Staging", "Data staging from PCGEx Asset Collections.",
		FName(TEXT("[ ") + MainCollection.GetAssetName() + TEXT(" ]")));
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscAdd; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UPCGExMeshCollection> MainCollection;

	/** Update point scale so staged asset fits within its bounds */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Bounds, meta=(PCG_Overridable))
	bool bUpdatePointScale = false;

	/** Update point scale so staged asset fits within its bounds */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Bounds, meta=(PCG_Overridable, EditCondition="bUpdatePointScale", EditConditionHides))
	bool bUniformScale = true;
	
	/** Update point bounds from staged data */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Bounds, meta=(PCG_Overridable))
	bool bUpdatePointBounds = true;

	/** Update point pivot to match staged bounds */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Bounds, meta=(PCG_Overridable))
	bool bUpdatePointPivot = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Distribution, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExSeedComponents"))
	uint8 SeedComponents = 0;

	/** Distribution type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Distribution, meta=(PCG_Overridable))
	EPCGExDistribution Distribution = EPCGExDistribution::WeightedRandom;

	/** Index sanitization behavior */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Distribution, meta=(PCG_Overridable, EditCondition="Distribution==EPCGExDistribution::Index", EditConditionHides))
	EPCGExIndexSafety IndexSafety = EPCGExIndexSafety::Tile;

	/** The name of the attribute index to read index selection from.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Distribution, meta=(PCG_Overridable, EditCondition="Distribution==EPCGExDistribution::Index", EditConditionHides))
	FPCGAttributePropertyInputSelector IndexSource;

	/** Note that this is only accounted for if selected in the seed component. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Distribution, meta=(PCG_Overridable))
	int32 LocalSeed = 0;

	/** The name of the attribute to write asset path to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Distribution, meta=(PCG_Overridable))
	FName AssetPathAttributeName = "AssetPath";

	/** If enabled, filter output based on whether a staging has been applied or not (empty entry). \n NOT IMPLEMENTED YET */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Distribution, meta=(PCG_Overridable))
	bool bOmitInvalidStagedPoints = false;

	/** Update point scale so staged asset fits within its bounds */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Staged Properties", meta=(PCG_Overridable))
	EPCGExWeightOutputMode WeightToAttribute = EPCGExWeightOutputMode::NoOutput;

	/** The name of the attribute to write asset weight to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Staged Properties", meta=(PCG_Overridable, EditCondition="WeightToAttribute!=EPCGExWeightOutputMode::NoOutput && WeightToAttribute!=EPCGExWeightOutputMode::NormalizedToDensity && WeightToAttribute!=EPCGExWeightOutputMode::NormalizedInvertedToDensity"))
	FName WeightAttributeName = "AssetWeight";
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAssetStagingContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExAssetStagingElement;

	virtual ~FPCGExAssetStagingContext() override;

	TObjectPtr<UPCGExMeshCollection> MainCollection;
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
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExAssetStaging
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		int32 NumPoints = 0;
		int32 MaxIndex = 0;
		bool bOutputWeight = false;
		bool bOneMinusWeight = false;
		bool bNormalizedWeight = false;

		const UPCGExAssetStagingSettings* LocalSettings = nullptr;
		const FPCGExAssetStagingContext* LocalTypedContext = nullptr;

		PCGExData::FCache<int32>* IndexGetter = nullptr;
		PCGEx::TFAttributeWriter<int32>* WeightWriter = nullptr;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
		PCGEx::TFAttributeWriter<FSoftObjectPath>* PathWriter = nullptr;
#else
		PCGEx::TFAttributeWriter<FString>* PathWriter = nullptr;
#endif

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}
