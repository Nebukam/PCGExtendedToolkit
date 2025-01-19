// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExUnionHelpers.h"
#include "PCGExEdgesProcessor.h"
#include "PCGExIntersections.h"
#include "Data/Blending/PCGExUnionBlender.h"
#include "Data/Blending/PCGExDataBlending.h"

#include "PCGExFuseClusters.generated.h"

namespace PCGExFuseClusters
{
	class FProcessor;
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExFuseClustersSettings : public UPCGExEdgesProcessorSettings
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
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;
	//~End UPCGExEdgesProcessorSettings interface

	/** Fuse Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Point/Point Settings"))
	FPCGExPointPointIntersectionDetails PointPointIntersectionDetails;

	/** Find Point-Edge intersection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bFindPointEdgeIntersections;

	/** Point-Edge intersection settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Point/Edge Settings", EditCondition="bFindPointEdgeIntersections"))
	FPCGExPointEdgeIntersectionDetails PointEdgeIntersectionDetails;

	/** Find Edge-Edge intersection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
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

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFuseClustersContext final : FPCGExEdgesProcessorContext
{
	friend class UPCGExFuseClustersSettings;
	friend class FPCGExFuseClustersElement;
	friend class PCGExFuseClusters::FProcessor;

	TArray<TSharedRef<PCGExData::FFacade>> VtxFacades;
	TSharedPtr<PCGExGraph::FUnionGraph> UnionGraph;
	TSharedPtr<PCGExData::FFacade> UnionDataFacade;

	FPCGExCarryOverDetails VtxCarryOverDetails;
	FPCGExCarryOverDetails EdgesCarryOverDetails;

	TSharedPtr<PCGExGraph::FUnionProcessor> UnionProcessor;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFuseClustersElement final : public FPCGExEdgesProcessorElement
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

namespace PCGExFuseClusters
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExFuseClustersContext, UPCGExFuseClustersSettings>
	{
		int32 VtxIOIndex = 0;
		int32 EdgesIOIndex = 0;
		TArray<PCGExGraph::FEdge> IndexedEdges;
		const TArray<FPCGPoint>* InPoints = nullptr;

	public:
		bool bInvalidEdges = true;
		TSharedPtr<PCGExGraph::FUnionGraph> UnionGraph;

		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
			bBuildCluster = false;
		}

		virtual ~FProcessor() override;

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		void InsertEdges(const PCGExMT::FScope& Scope);
		void OnInsertionComplete();
	};

}
