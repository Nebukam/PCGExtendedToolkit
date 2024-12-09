// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExClusterMT.h"
#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExTopologyEdgesProcessor.h"
#include "PCGExTopologyClusterSurface.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTopologyClusterSurfaceSettings : public UPCGExTopologyEdgesProcessorSettings
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

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTopologyClusterSurfaceContext final : FPCGExTopologyEdgesProcessorContext
{
	friend class FPCGExTopologyClusterSurfaceElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTopologyClusterSurfaceElement final : public FPCGExTopologyEdgesProcessorElement
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
		virtual void PrepareSingleLoopScopeForEdges(const PCGExMT::FScope& Scope) override;
		virtual void ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FEdge& Edge, const PCGExMT::FScope& Scope) override;
		bool FindCell(const PCGExCluster::FNode& Node, const PCGExGraph::FEdge& Edge, int32 LoopIdx, const bool bSkipBinary = true);
		void EnsureRoamingClosedLoopProcessing();
		virtual void OnEdgesProcessingComplete() override;
	};
}
