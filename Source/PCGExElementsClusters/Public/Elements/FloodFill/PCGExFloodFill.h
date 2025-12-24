// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExClustersProcessor.h"
#include "PCGExFloodFill.generated.h"

namespace PCGEx
{
	class FHashLookupMap;
}

namespace PCGExBlending
{
	class FBlendOpsManager;
}

class FPCGExBlendOperation;
class UPCGExFillControlsFactoryData;
class FPCGExFillControlOperation;

UENUM()
enum class EPCGExFloodFillSettingSource : uint8
{
	Seed = 0 UMETA(DisplayName = "Seed", ToolTip="Read values from seed point."),
	Vtx  = 1 UMETA(DisplayName = "Vtx", ToolTip="Read values from vtx point."),
};

UENUM()
enum class EPCGExFloodFillPrioritization : uint8
{
	Heuristics = 0 UMETA(DisplayName = "Heuristics", ToolTip="Prioritize expansion based on heuristics first, then depth."),
	Depth      = 1 UMETA(DisplayName = "Depth", ToolTip="Prioritize expansion based on depth, then FillControls."),
};

UENUM(meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Flood Fill Control Step Flags"))
enum class EPCGExFloodFillControlStepsFlags : uint8
{
	None      = 0,
	Capture   = 1 << 0 UMETA(DisplayName = "Capture", ToolTip="When a node is captured by a diffusion."),
	Probing   = 1 << 1 UMETA(DisplayName = "Probing", ToolTip="When captured, a node is then 'probed', iterating through unvisited neighbors and registering them as candidates."),
	Candidate = 1 << 2 UMETA(DisplayName = "Candidate", ToolTip="When a node is identified as candidate to be flooded (e.g neighbor of a captured node)."),
};

ENUM_CLASS_FLAGS(EPCGExFloodFillControlStepsFlags)
using EPCGExFloodFillControlStepsFlagsBitmask = TEnumAsByte<EPCGExFloodFillControlStepsFlags>;

UENUM(meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Diffusion FillControl Flags"))
enum class EPCGExFloodFillHeuristicFlags : uint8
{
	None          = 0,
	LocalScore    = 1 << 0 UMETA(DisplayName = "Local Score", ToolTip="From neighbor to neighbor"),
	GlobalScore   = 1 << 1 UMETA(DisplayName = "Global Score", ToolTip="From seed to candidate"),
	PreviousScore = 1 << 2 UMETA(DisplayName = "Previous Score", ToolTip="Previously accumulated local score"),
};

ENUM_CLASS_FLAGS(EPCGExFloodFillHeuristicFlags)
using EPCGExFloodFillHeuristicFlagsBitmask = TEnumAsByte<EPCGExFloodFillHeuristicFlags>;

USTRUCT(BlueprintType)
struct PCGEXELEMENTSCLUSTERS_API FPCGExFloodFillFlowDetails
{
	GENERATED_BODY()

	FPCGExFloodFillFlowDetails()
	{
	}

	/** Which data should be prioritized to 'drive' diffusion */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExFloodFillPrioritization Priority = EPCGExFloodFillPrioritization::Heuristics;

	/** Diffusion Rate type.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExFloodFillSettingSource FillRateSource = EPCGExFloodFillSettingSource::Seed;

	/** Diffusion Rate type.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExInputValueType FillRateInput = EPCGExInputValueType::Constant;

	/** Fetch the Diffusion Rate from a local attribute. Must be >= 0, but zero wont grow -- it will however "preserve" the vtx from being diffused on. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Fill Rate (Attr)", EditCondition="FillRateInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName FillRateAttribute = FName("FillRate");

	/** Diffusion rate constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Fill Rate", EditCondition="FillRateInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0))
	int32 FillRateConstant = 1;

	/** What components are used for scoring points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExElementsClusters.EPCGExFloodFillHeuristicFlags"))
	uint8 Scoring = static_cast<uint8>(EPCGExFloodFillHeuristicFlags::LocalScore);
};

namespace PCGExFloodFill
{
	const FName OutputFillControlsLabel = TEXT("Fill Control");
	const FName SourceFillControlsLabel = TEXT("Fill Controls");

	struct FCandidate
	{
		const PCGExClusters::FNode* Node = nullptr;
		PCGExGraphs::FLink Link;
		int32 CaptureIndex = -1;
		int32 Depth = 0;
		double PathScore = 0;
		double Score = 0;
		double PathDistance = 0;
		double Distance = 0;

		FCandidate() = default;
	};

	class FFillControlsHandler;

	class FDiffusion : public TSharedFromThis<FDiffusion>
	{
	protected:
		TSet<int32> Visited;
		// use map hash lookup to reduce memory overhead of a shared map + thread safety yay

		int32 MaxDepth = 0;
		double MaxDistance = 0;

		TSharedPtr<FFillControlsHandler> FillControlsHandler;

	public:
		int32 Index = -1;
		bool bStopped = false;
		const PCGExClusters::FNode* SeedNode = nullptr;
		int32 SeedIndex = -1;
		TSet<int32> Endpoints;

		TSharedPtr<PCGEx::FHashLookupMap> TravelStack; // Required for FillControls & Heuristics
		TSharedPtr<PCGExClusters::FCluster> Cluster;

		TArray<FCandidate> Candidates;
		TArray<FCandidate> Captured;

		FDiffusion(const TSharedPtr<FFillControlsHandler>& InFillControlsHandler, const TSharedPtr<PCGExClusters::FCluster>& InCluster, const PCGExClusters::FNode* InSeedNode);
		~FDiffusion() = default;

		int32 GetSettingsIndex(EPCGExFloodFillSettingSource Source) const;

		void Init(const int32 InSeedIndex);
		void Probe(const FCandidate& From);
		void Grow();
		void PostGrow();

		void Diffuse(const TSharedPtr<PCGExData::FFacade>& InVtxFacade, const TSharedPtr<PCGExBlending::FBlendOpsManager>& InBlendOps, TArray<int32>& OutIndices);
	};

	class PCGEXELEMENTSCLUSTERS_API FFillControlsHandler : public TSharedFromThis<FFillControlsHandler>
	{
	protected:
		FPCGExContext* ExecutionContext = nullptr;
		bool bIsValidHandler = false;
		int32 NumDiffusions = 0;

		// Subselections
		TArray<TSharedPtr<FPCGExFillControlOperation>> SubOpsProbe;
		TArray<TSharedPtr<FPCGExFillControlOperation>> SubOpsCandidate;
		TArray<TSharedPtr<FPCGExFillControlOperation>> SubOpsCapture;

	public:
		mutable FRWLock HandlerLock;

		TSharedPtr<PCGExClusters::FCluster> Cluster;
		TSharedPtr<PCGExData::FFacade> VtxDataFacade;
		TSharedPtr<PCGExData::FFacade> EdgeDataFacade;
		TSharedPtr<PCGExData::FFacade> SeedsDataFacade;
		TWeakPtr<PCGExHeuristics::FHandler> HeuristicsHandler;

		TArray<TSharedPtr<FPCGExFillControlOperation>> Operations;

		TSharedPtr<TArray<int8>> InfluencesCount;

		TSharedPtr<TArray<int32>> SeedIndices;
		TSharedPtr<TArray<int32>> SeedNodeIndices;

		EPCGExFloodFillPrioritization Sorting = EPCGExFloodFillPrioritization::Depth;

		bool bUseLocalScore = false;
		bool bUseGlobalScore = false;
		bool bUsePreviousScore = false;

		FORCEINLINE bool IsValidHandler() const { return bIsValidHandler; }
		FORCEINLINE int32 GetNumDiffusions() const { return NumDiffusions; }

		FFillControlsHandler(FPCGExContext* InContext, const TSharedPtr<PCGExClusters::FCluster>& InCluster, const TSharedPtr<PCGExData::FFacade>& InVtxDataCache, const TSharedPtr<PCGExData::FFacade>& InEdgeDataCache, const TSharedPtr<PCGExData::FFacade>& InSeedsDataCache, const TArray<TObjectPtr<const UPCGExFillControlsFactoryData>>& InFactories);

		~FFillControlsHandler();

		bool BuildFrom(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExFillControlsFactoryData>>& InFactories);

		bool PrepareForDiffusions(const TArray<TSharedPtr<FDiffusion>>& Diffusions, const FPCGExFloodFillFlowDetails& Details);

		bool TryCapture(const FDiffusion* Diffusion, const FCandidate& Candidate);
		bool IsValidProbe(const FDiffusion* Diffusion, const FCandidate& Candidate);
		bool IsValidCandidate(const FDiffusion* Diffusion, const FCandidate& From, const FCandidate& Candidate);
	};
}
