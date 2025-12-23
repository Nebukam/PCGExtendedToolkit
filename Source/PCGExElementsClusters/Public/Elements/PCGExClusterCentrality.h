// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Details/PCGExDetailsNoise.h"
#include "Core/PCGExClustersProcessor.h"

#include "PCGExClusterCentrality.generated.h"

namespace PCGEx
{
	class FScoredQueue;
}

class UPCGExSearchInstancedFactory;

namespace PCGExMT
{
	template <typename T>
	class TScopedArray;
}

UENUM()
enum class EPCGExCentralityDownsampling : uint8
{
	None    = 0 UMETA(DisplayName = "None", ToolTip="All connected filters must pass."),
	Ratio   = 1 UMETA(DisplayName = "Random ratio", ToolTip="Sample using a random subset of the nodes."),
	Filters = 2 UMETA(DisplayName = "Filters", ToolTip="Use filters to drive which nodes are added to the subset")
};

/**
 * Use PCGExTransform to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="clusters/centrality"))
class UPCGExClusterCentralitySettings : public UPCGExClustersProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	virtual void ApplyDeprecation(UPCGNode* InOutNode) override;

	PCGEX_NODE_INFOS(ClusterCentrality, "Cluster : Centrality", "Compute betweenness centrality. Processing time increases exponentially with the number of vtx.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(NeighborSampler); }
#endif

	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const override;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

	/** Name of the attribute */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName CentralityValueAttributeName = FName("Centrality");

	/** Discrete mode write the number as-is, relative will normalize against the highest number of overlaps found. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Normalize", EditConditionHides))
	bool bNormalize = true;

	/** Whether to do a OneMinus on the normalized overlap count value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ OneMinus", EditCondition="bNormalize", EditConditionHides))
	bool bOutputOneMinus = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExCentralityDownsampling DownsamplingMode = EPCGExCentralityDownsampling::None;

	/** If enabled, only compute centrality on a subset of the nodes to get a rough approximation. This is useful for large clusters, or if you want to tradeoff precision for speed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Ratio", EditCondition="DownsamplingMode == EPCGExCentralityDownsampling::Ratio", EditConditionHides))
	FPCGExRandomRatioDetails RandomDownsampling;
};

struct FPCGExClusterCentralityContext final : FPCGExClustersProcessorContext
{
	friend class FPCGExClusterCentralityElement;

	TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> VtxFilterFactories;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExClusterCentralityElement final : public FPCGExClustersProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ClusterCentrality)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExClusterCentrality
{
	using NodePred = TArray<int32, TInlineAllocator<8>>;

	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExClusterCentralityContext, UPCGExClusterCentralitySettings>
	{
		friend class FBatch;

	protected:
		bool bDownsample = false;

		FRWLock CompletionLock;
		bool bVtxComplete = true;
		bool bEdgeComplete = false;

		TArray<int32> RandomSamples;
		TArray<double> DirectedEdgeScores;
		TArray<double> Betweenness;
		TSharedPtr<PCGExMT::TScopedArray<double>> ScopedBetweenness;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;

		virtual void ProcessEdges(const PCGExMT::FScope& Scope) override;
		virtual void OnEdgesProcessingComplete() override;

		virtual void ProcessNodes(const PCGExMT::FScope& Scope) override;
		virtual void OnNodesProcessingComplete() override;

		void TryStartCompute();

		virtual void PrepareLoopScopesForRanges(const TArray<PCGExMT::FScope>& Loops) override;
		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
		virtual void OnRangeProcessingComplete() override;

		void ProcessSingleNode(const int32 Index, TArray<double>& LocalBetweenness, TArray<double>& Score, TArray<double>& Sigma, TArray<double>& Delta, TArray<NodePred>& Pred, TArray<int32>& Stack, const TSharedPtr<PCGEx::FScoredQueue>& Queue);
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		friend class FProcessor;

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);

		virtual void Write() override;
	};
}
