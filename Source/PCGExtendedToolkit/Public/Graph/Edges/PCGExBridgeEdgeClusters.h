// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "PCGExBridgeEdgeClusters.generated.h"

UENUM(BlueprintType)
enum class EPCGExBridgeClusterMethod : uint8
{
	Delaunay UMETA(DisplayName = "Delaunay", ToolTip="Uses Delaunay graph to find connections."),
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
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	virtual bool GetCacheAllClusters() const override;
	//~End UPCGExPointsProcessorSettings interface

	virtual bool GetGenerateClusters() const override;
	
	/** Method used to find & insert bridges */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExBridgeClusterMethod BridgeMethod = EPCGExBridgeClusterMethod::Delaunay;

private:
	friend class FPCGExBridgeEdgeClustersElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBridgeEdgeClustersContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExBridgeEdgeClustersElement;

	virtual ~FPCGExBridgeEdgeClustersContext() override;

	EPCGExBridgeClusterMethod BridgeMethod;

	int32 TotalPoints = -1;
	PCGExData::FPointIO* ConsolidatedEdges = nullptr;
	TSet<PCGExCluster::FCluster*> VisitedClusters;
	
	FPCGExPointIOMerger* Merger = nullptr;
	PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;

	TMap<int32, int32> CachedPointIndices;
	PCGEx::TFAttributeReader<int32>* StartIndexReader = nullptr;
	PCGEx::TFAttributeReader<int32>* EndIndexReader = nullptr;
	
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

// Define the background task class
class PCGEXTENDEDTOOLKIT_API FPCGExBridgeClusteresTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExBridgeClusteresTask(
		FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO, const int32 InOtherClusterIndex) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		OtherClusterIndex(InOtherClusterIndex)
	{
	}

	int32 OtherClusterIndex = -1;

	virtual bool ExecuteTask() override;
};
