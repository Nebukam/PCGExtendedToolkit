// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "PCGExBridgeEdgeClusters.generated.h"

class FPCGExPointIOMerger;

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Bridge Cluster Mode"))
enum class EPCGExBridgeClusterMethod : uint8
{
	Delaunay3D UMETA(DisplayName = "Delaunay 3D", ToolTip="Uses Delaunay 3D graph to find connections."),
	Delaunay2D UMETA(DisplayName = "Delaunay 2D", ToolTip="Uses Delaunay 2D graph to find connections."),
	LeastEdges UMETA(DisplayName = "Least Edges", ToolTip="Ensure all clusters are connected using the least possible number of bridges."),
	MostEdges UMETA(DisplayName = "Most Edges", ToolTip="Each cluster will have a bridge to every other cluster"),
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExBridgeEdgeClustersSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExBridgeEdgeClustersSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BridgeEdgeClusters, "Graph : Bridge Clusters", "Connects isolated edge clusters by their closest vertices.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorGraph; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

	/** Method used to find & insert bridges */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExBridgeClusterMethod BridgeMethod = EPCGExBridgeClusterMethod::Delaunay3D;

	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="BridgeMethod==EPCGExBridgeClusterMethod::Delaunay2D", EditConditionHides))
	FPCGExGeo2DProjectionSettings ProjectionSettings = FPCGExGeo2DProjectionSettings(false);

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Graph Output Settings"))
	FPCGExGraphBuilderSettings GraphBuilderSettings;

private:
	friend class FPCGExBridgeEdgeClustersElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBridgeEdgeClustersContext final : public FPCGExEdgesProcessorContext
{
	friend class FPCGExBridgeEdgeClustersElement;
	friend class FPCGExCreateBridgeTask;

	virtual ~FPCGExBridgeEdgeClustersContext() override;

	FPCGExGeo2DProjectionSettings ProjectionSettings;
};

class PCGEXTENDEDTOOLKIT_API FPCGExBridgeEdgeClustersElement final : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExBridgeClusters
{
	class FProcessor final : public PCGExClusterMT::FClusterProcessor
	{
	public:
		FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
			FClusterProcessor(InVtx, InEdges)
		{
		}

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSingleEdge(PCGExGraph::FIndexedEdge& Edge) override;
		virtual void CompleteWork() override;
	};

	class FProcessorBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
	public:
		PCGExData::FPointIO* ConsolidatedEdges = nullptr;
		FPCGExPointIOMerger* Merger = nullptr;
		TSet<uint64> Bridges;

		TArray<PCGExCluster::FCluster*> ValidClusters;

		FProcessorBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, TArrayView<PCGExData::FPointIO*> InEdges);
		virtual ~FProcessorBatch() override;

		virtual bool PrepareProcessing() override;
		virtual bool PrepareSingle(FProcessor* ClusterProcessor) override;
		virtual void CompleteWork() override;

		void ConnectClusters();
		void Write() const;
	};

	class PCGEXTENDEDTOOLKIT_API FPCGExCreateBridgeTask final : public PCGExMT::FPCGExTask
	{
	public:
		FPCGExCreateBridgeTask(
			PCGExData::FPointIO* InPointIO,
			FProcessorBatch* InBatch,
			PCGExCluster::FCluster* A,
			PCGExCluster::FCluster* B) :
			PCGExMT::FPCGExTask(InPointIO),
			Batch(InBatch),
			ClusterA(A),
			ClusterB(B)
		{
		}

		FProcessorBatch* Batch = nullptr;

		PCGExCluster::FCluster* ClusterA = nullptr;
		PCGExCluster::FCluster* ClusterB = nullptr;

		virtual bool ExecuteTask() override;

		void BumpEdgeNum(const FPCGPoint& A, const FPCGPoint& B) const;
	};
}
