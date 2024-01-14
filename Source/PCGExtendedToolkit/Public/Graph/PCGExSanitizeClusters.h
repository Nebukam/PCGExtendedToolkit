// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "IPCGExDebug.h"
#include "PCGExCluster.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExData.h"

#include "PCGExSanitizeClusters.generated.h"

#define PCGEX_INVALID_CLUSTER_LOG PCGE_LOG(Warning, GraphAndLog, FTEXT("Some clusters are corrupted and will be ignored. If you modified vtx/edges manually, make sure to use Sanitize Cluster first."));

/**
 * A Base node to process a set of point using GraphParams.
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExSanitizeClustersSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SanitizeClusters, "Edges : Sanitize Clusters", "Ensure the input set of vertex and edges outputs clean, interconnected clusters. May create new clusters, but does not creates nor deletes points/edges.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorEdge; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;

	virtual FName GetMainInputLabel() const override;
	virtual FName GetMainOutputLabel() const override;
	//~End UPCGExPointsProcessorSettings interface

	virtual bool GetCacheAllClusters() const;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSanitizeClustersContext : public FPCGExPointsProcessorContext
{
	friend class UPCGExSanitizeClustersSettings;
	friend class FPCGExSanitizeClustersElement;

	virtual ~FPCGExSanitizeClustersContext() override;

	PCGExData::FPointIOGroup* MainEdges = nullptr;
	PCGExData::FPointIO* CurrentEdges = nullptr;

	PCGExData::FPointIOTaggedDictionary* InputDictionary = nullptr;
	PCGExData::FPointIOTaggedEntries* TaggedEdges = nullptr;

	TMap<int32, int32> CachedPointIndices;
	PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
	
	virtual bool AdvancePointsIO() override;
	bool AdvanceEdges(); // Advance edges within current points

	void OutputPointsAndEdges();

protected:
	int32 CurrentEdgesIndex = -1;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSanitizeClustersElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual FPCGContext* InitializeContext(
		FPCGExPointsProcessorContext* InContext,
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;	
};