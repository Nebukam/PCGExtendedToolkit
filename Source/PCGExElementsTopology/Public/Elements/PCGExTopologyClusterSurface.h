// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExClusterMT.h"
#include "Core/PCGExClustersProcessor.h"
#include "Core/PCGExTopologyClustersProcessor.h"

#include "PCGExTopologyClusterSurface.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(Keywords = "collision"), meta=(PCGExNodeLibraryDoc="topology/cluster-surface"))
class UPCGExTopologyClusterSurfaceSettings : public UPCGExTopologyClustersProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(TopologyClusterSurface, "Topology : Cluster Surface", "Create a cluster surface topology");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

private:
	friend class FPCGExTopologyClustersProcessorElement;
};

struct FPCGExTopologyClusterSurfaceContext final : FPCGExTopologyClustersProcessorContext
{
	friend class FPCGExTopologyClusterSurfaceElement;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExTopologyClusterSurfaceElement final : public FPCGExTopologyClustersProcessorElement
{
public:
	// Generates artifacts
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(TopologyClusterSurface)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExTopologyClusterSurface
{
	class FProcessor final : public PCGExTopologyEdges::TProcessor<FPCGExTopologyClusterSurfaceContext, UPCGExTopologyClusterSurfaceSettings>
	{
		TArray<TSharedRef<TArray<FGeometryScriptSimplePolygon>>> SubTriangulations;
		int32 NumAttempts = 0;
		int32 LastBinary = -1;
		int32 NumTriangulations = 0;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual void CompleteWork() override;
		virtual void PrepareLoopScopesForEdges(const TArray<PCGExMT::FScope>& Loops) override;
		virtual void ProcessEdges(const PCGExMT::FScope& Scope) override;
		bool FindCell(const PCGExClusters::FNode& Node, const PCGExGraphs::FEdge& Edge, int32 LoopIdx, const bool bSkipBinary = true);
		void EnsureRoamingClosedLoopProcessing();
		virtual void OnEdgesProcessingComplete() override;
	};

	class FBatch final : public PCGExTopologyEdges::TBatch<FProcessor>
	{
	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);
	};
}
