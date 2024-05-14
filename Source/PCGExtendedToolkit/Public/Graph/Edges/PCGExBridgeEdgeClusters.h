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
	PCGEX_NODE_INFOS(BridgeEdgeClusters, "Edges : Bridge Clusters", "Connects isolated edge clusters by their closest vertices.");
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

private:
	friend class FPCGExBridgeEdgeClustersElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBridgeEdgeClustersContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExBridgeEdgeClustersElement;
	friend class FPCGExCreateBridgeTask;

	virtual ~FPCGExBridgeEdgeClustersContext() override;

	EPCGExBridgeClusterMethod BridgeMethod;
	FPCGExGeo2DProjectionSettings ProjectionSettings;

	int32 TotalPoints = -1;
	PCGExData::FPointIO* ConsolidatedEdges = nullptr;

	TArray<PCGExCluster::FCluster*> Clusters;
	TArray<PCGExData::FPointIO*> BridgedEdges;
	TArray<PCGExCluster::FCluster*> BridgedClusters;

	FPCGExPointIOMerger* Merger = nullptr;
	FPCGExGraphBuilderSettings GraphBuilderSettings;
	PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;

protected:
	mutable FRWLock NumEdgeLock;
	void BumpEdgeNum(const FPCGPoint& A, const FPCGPoint& B) const;
};

class PCGEXTENDEDTOOLKIT_API FPCGExBridgeEdgeClustersElement : public FPCGExEdgesProcessorElement
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

class PCGEXTENDEDTOOLKIT_API FPCGExCreateBridgeTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExCreateBridgeTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
	                       PCGExCluster::FCluster* A,
	                       PCGExCluster::FCluster* B) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		ClusterA(A),
		ClusterB(B)
	{
	}

	PCGExCluster::FCluster* ClusterA = nullptr;
	PCGExCluster::FCluster* ClusterB = nullptr;

	virtual bool ExecuteTask() override;
};
