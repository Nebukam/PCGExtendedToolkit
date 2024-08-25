// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExClusterMT.h"
#include "PCGExEdgesProcessor.h"

#include "PCGExPartitionVertices.generated.h"


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPartitionVerticesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PartitionVertices, "Cluster : Partition Vtx", "Split Vtx into per-cluster groups.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorCluster; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExEdgesProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End UPCGExEdgesProcessorSettings interface
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPartitionVerticesContext final : public FPCGExEdgesProcessorContext
{
	friend class UPCGExPartitionVerticesSettings;
	friend class FPCGExPartitionVerticesElement;

	virtual ~FPCGExPartitionVerticesContext() override;

	PCGExData::FPointIOCollection* VtxPartitions = nullptr;
	TArray<PCGExGraph::FIndexedEdge> IndexedEdges;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPartitionVerticesElement final : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExPartitionVertices
{
	class FProcessor final : public PCGExClusterMT::FClusterProcessor
	{
		friend class FProcessorBatch;

	protected:
		virtual PCGExCluster::FCluster* HandleCachedCluster(const PCGExCluster::FCluster* InClusterRef) override;

		FPCGExPartitionVerticesContext* LocalTypedContext = nullptr;

		PCGExData::FPointIO* PointPartitionIO = nullptr;
		TMap<int32, int32> Remapping;
		TArray<int32> KeptIndices;

	public:
		FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
			FClusterProcessor(InVtx, InEdges)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node) override;
		virtual void ProcessSingleEdge(PCGExGraph::FIndexedEdge& Edge) override;
		virtual void CompleteWork() override;
	};
}
