// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExEdgesProcessor.h"
#include "PCGExIntersections.h"
#include "Data/Blending/PCGExDataBlending.h"

#include "PCGExFuseClusters.generated.h"

namespace PCGExDataBlending
{
	class FCompoundBlender;
}

namespace PCGExGraph
{
	struct FCompoundProcessor;
	struct FCompoundGraph;
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExFuseClustersSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FuseClusters, "Cluster : Fuse", "Finds Point/Edge and Edge/Edge intersections between all input clusters.");
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

	/** Fuse Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Point/Point Settings"))
	FPCGExPointPointIntersectionDetails PointPointIntersectionDetails;

	/** Find Point-Edge intersection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bFindPointEdgeIntersections;

	/** Point-Edge intersection settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Point/Edge Settings", EditCondition="bFindPointEdgeIntersections"))
	FPCGExPointEdgeIntersectionDetails PointEdgeIntersectionDetails;

	/** Find Edge-Edge intersection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bFindEdgeEdgeIntersections;

	/** Edge-Edge intersection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Edge/Edge Settings", EditCondition="bFindEdgeEdgeIntersections"))
	FPCGExEdgeEdgeIntersectionDetails EdgeEdgeIntersectionDetails;

	/** Defines how fused point properties and attributes are merged together for fused points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta=(PCG_Overridable))
	FPCGExBlendingDetails DefaultPointsBlendingDetails;

	/** Defines how fused point properties and attributes are merged together for fused edges. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta=(PCG_Overridable))
	FPCGExBlendingDetails DefaultEdgesBlendingDetails;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta=(PCG_Overridable))
	bool bUseCustomPointEdgeBlending = false;

	/** Defines how fused point properties and attributes are merged together for Point/Edge intersections. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta=(PCG_Overridable, EditCondition="bUseCustomPointEdgeBlending"))
	FPCGExBlendingDetails CustomPointEdgeBlendingDetails;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta=(PCG_Overridable))
	bool bUseCustomEdgeEdgeBlending = false;

	/** Defines how fused point properties and attributes are merged together for Edge/Edge intersections (Crossings). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta=(PCG_Overridable, EditCondition="bUseCustomEdgeEdgeBlending"))
	FPCGExBlendingDetails CustomEdgeEdgeBlendingDetails;


	/** Meta filter settings for Vtx. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Meta Filters", meta = (PCG_Overridable, DisplayName="Carry Over Settings - Vtx"))
	FPCGExCarryOverDetails VtxCarryOverDetails;

	/** Meta filter settings for Edges. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Meta Filters", meta = (PCG_Overridable, DisplayName="Carry Over Settings - Edges"))
	FPCGExCarryOverDetails EdgesCarryOverDetails;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFuseClustersContext final : public FPCGExEdgesProcessorContext
{
	friend class UPCGExFuseClustersSettings;
	friend class FPCGExFuseClustersElement;

	virtual ~FPCGExFuseClustersContext() override;

	TArray<PCGExData::FFacade*> VtxFacades;
	PCGExGraph::FCompoundGraph* CompoundGraph = nullptr;
	PCGExData::FFacade* CompoundFacade = nullptr;

	FPCGExCarryOverDetails VtxCarryOverDetails;
	FPCGExCarryOverDetails EdgesCarryOverDetails;

	PCGExDataBlending::FCompoundBlender* CompoundEdgesBlender = nullptr;

	PCGExGraph::FCompoundProcessor* CompoundProcessor = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExFuseClustersElement final : public FPCGExEdgesProcessorElement
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

namespace PCGExFuseClusters
{
	class FProcessor final : public PCGExClusterMT::FClusterProcessor
	{
		int32 VtxIOIndex = 0;
		int32 EdgesIOIndex = 0;
		TArray<PCGExGraph::FIndexedEdge> IndexedEdges;
		const TArray<FPCGPoint>* InPoints = nullptr;

	public:
		bool bInvalidEdges = true;
		PCGExGraph::FCompoundGraph* CompoundGraph = nullptr;

		explicit FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges)
			: FClusterProcessor(InVtx, InEdges)
		{
			bBuildCluster = false;
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		FORCEINLINE virtual void ProcessSingleRangeIteration(const int32 Iteration) override
		{
			ProcessSingleEdge(IndexedEdges[Iteration]);
		}

		FORCEINLINE virtual void ProcessSingleEdge(PCGExGraph::FIndexedEdge& Edge) override
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFusePointsElement::ProcessSingleEdge);

			CompoundGraph->InsertEdge(
				*(InPoints->GetData() + Edge.Start), VtxIOIndex, Edge.Start,
				*(InPoints->GetData() + Edge.End), VtxIOIndex, Edge.End,
				EdgesIOIndex, Edge.PointIndex);
		}

		virtual void CompleteWork() override;
	};
}
