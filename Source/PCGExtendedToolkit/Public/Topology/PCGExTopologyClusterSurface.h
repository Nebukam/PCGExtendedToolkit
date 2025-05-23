// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExClusterMT.h"
#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExTopologyEdgesProcessor.h"
#include "PCGExTopologyClusterSurface.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(Keywords = "collision"))
class UPCGExTopologyClusterSurfaceSettings : public UPCGExTopologyEdgesProcessorSettings
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
	friend class FPCGExTopologyEdgesProcessorElement;
};

struct FPCGExTopologyClusterSurfaceContext final : FPCGExTopologyEdgesProcessorContext
{
	friend class FPCGExTopologyClusterSurfaceElement;
};

class FPCGExTopologyClusterSurfaceElement final : public FPCGExTopologyEdgesProcessorElement
{
public:
	// Generates artifacts
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	PCGEX_CAN_ONLY_EXECUTE_ON_MAIN_THREAD(true)

	PCGEX_ELEMENT_CREATE_CONTEXT(TopologyClusterSurface)
	
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
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
		bool FindCell(const PCGExCluster::FNode& Node, const PCGExGraph::FEdge& Edge, int32 LoopIdx, const bool bSkipBinary = true);
		void EnsureRoamingClosedLoopProcessing();
		virtual void OnEdgesProcessingComplete() override;
	};
}
