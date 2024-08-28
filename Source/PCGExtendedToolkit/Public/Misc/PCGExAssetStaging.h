// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "AssetSelectors/PCGExMeshCollection.h"
#include "PCGExAssetStaging.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Bounds Staging"))
enum class EPCGExBoundsStaging : uint8
{
	Ignore UMETA(DisplayName = "Ignore", ToolTip="Ignore bounds"),
	UpdatePointScale UMETA(DisplayName = "Update scale", ToolTip="Update the point scale so final asset matches the existing point' bounds"),
	UpdatePointBounds UMETA(DisplayName = "Update bounds", ToolTip="Update the point bounds so it reflects the bounds of the final asset"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Bounds Staging"))
enum class EPCGExDistribution : uint8
{
	Index UMETA(DisplayName = "Index", ToolTip="Distribution by index"),
	Random UMETA(DisplayName = "Random", ToolTip="Update the point scale so final asset matches the existing point' bounds"),
	WeightedRandom UMETA(DisplayName = "Weighted random", ToolTip="Update the point bounds so it reflects the bounds of the final asset"),
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
		FName(MainCollection.GetAssetName()));
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscWrite; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExSeedComponents"))
	uint8 SeedComponents = 0;

	/** Distribution type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDistribution Distribution = EPCGExDistribution::WeightedRandom;

	/** Index sanitization behavior */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Distribution==EPCGExDistribution::Index", EditConditionHides))
	EPCGExIndexSafety IndexSafety = EPCGExIndexSafety::Tile;

	/** The name of the attribute index to read index selection from.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Distribution==EPCGExDistribution::Index", EditConditionHides))
	FPCGAttributePropertyInputSelector IndexAttribute;

	/** Note that this is only accounted for if selected in the seed component. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int32 LocalSeed = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UPCGExMeshCollection> MainCollection;

	/** How to handle bounds */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExBoundsStaging BoundsStaging = EPCGExBoundsStaging::UpdatePointBounds;

	/** The name of the attribute to write asset path to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName AssetPathAttributeName = "AssetPath";

	/** If enabled, filter output based on whether a staging has been applied or not (empty entry). \n NOT IMPLEMENTED YET */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bOmitInvalidStagedPoints = false;
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
		double NumPoints = 0;

		const UPCGExAssetStagingSettings* LocalSettings = nullptr;
		const FPCGExAssetStagingContext* LocalTypedContext = nullptr;

		PCGExData::FCache<int32>* IndexCache = nullptr;

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
