// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Clusters/Artifacts/PCGExCellDetails.h"
#include "Core/PCGExClustersProcessor.h"
#include "Details/PCGExBlendingDetails.h"
#include "Graphs/PCGExGraphDetails.h"

#include "PCGExBuildCellDiagram.generated.h"

namespace PCGExBlending
{
	class FUnionBlender;
}

namespace PCGExClusters
{
	class FProjectedPointSet;
	class FCellConstraints;
	class FCell;
}

namespace PCGExGraphs
{
	class FGraphBuilder;
}

namespace PCGExBuildCellDiagram
{
	class FProcessor;
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class UPCGExBuildCellDiagramSettings : public UPCGExClustersProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BuildCellDiagram, "Cluster : Cell Diagram", "Creates a graph from cell adjacency relationships. Points are cell centroids, edges connect adjacent cells.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_BLEND(ClusterGenerator, Pathfinding); }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings


	/** Cell constraints for filtering which cells become graph nodes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExCellConstraintsDetails Constraints = FPCGExCellConstraintsDetails(false);

	/** Projection settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionDetails ProjectionDetails;

	/** Graph output settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGraphBuilderDetails GraphBuilderDetails;

	/** Write cell area to centroid points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Attributes", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteArea = false;

	/** Attribute name for cell area */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Attributes", meta = (PCG_Overridable, EditCondition="bWriteArea"))
	FName AreaAttributeName = FName("Area");

	/** Write cell compactness to centroid points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Attributes", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteCompactness = false;

	/** Attribute name for cell compactness */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Attributes", meta = (PCG_Overridable, EditCondition="bWriteCompactness"))
	FName CompactnessAttributeName = FName("Compactness");

	/** Write number of nodes in cell to centroid points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Attributes", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteNumNodes = false;

	/** Attribute name for node count */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Attributes", meta = (PCG_Overridable, EditCondition="bWriteNumNodes"))
	FName NumNodesAttributeName = FName("NumNodes");

	/** Defines how cell vertex properties and attributes are blended to the centroid. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Blending", meta = (PCG_Overridable))
	FPCGExBlendingDetails BlendingDetails = FPCGExBlendingDetails(EPCGExBlendingType::Average, EPCGExBlendingType::None);

	/** Meta filter settings for attribute carry-over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Blending", meta = (PCG_Overridable, DisplayName="Carry Over Settings"))
	FPCGExCarryOverDetails CarryOverDetails;

private:
	friend class FPCGExBuildCellDiagramElement;
};

struct FPCGExBuildCellDiagramContext final : FPCGExClustersProcessorContext
{
	friend class FPCGExBuildCellDiagramElement;

	TSharedPtr<PCGExClusters::FProjectedPointSet> Holes;
	TSharedPtr<PCGExData::FFacade> HolesFacade;
	FPCGExCarryOverDetails CarryOverDetails;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExBuildCellDiagramElement final : public FPCGExClustersProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(BuildCellDiagram)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExBuildCellDiagram
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExBuildCellDiagramContext, UPCGExBuildCellDiagramSettings>
	{
	protected:
		
		TSharedPtr<PCGExData::FFacade> CentroidFacade;
		
		TSharedPtr<PCGExClusters::FProjectedPointSet> Holes;
		TArray<TSharedPtr<PCGExClusters::FCell>> ValidCells;
		TSharedPtr<PCGExGraphs::FGraphBuilder> GraphBuilder;

		// Cell adjacency
		TMap<int32, TSet<int32>> CellAdjacencyMap;
		TMap<int32, int32> FaceIndexToOutputIndex; // Maps face index to output point index
		
		TSharedPtr<PCGExBlending::FUnionBlender> UnionBlender; 
			
		TSharedPtr<PCGExData::TBuffer<double>> AreaWriter = nullptr;
		TSharedPtr<PCGExData::TBuffer<double>> CompactnessWriter = nullptr;
		TSharedPtr<PCGExData::TBuffer<int32>> NumNodesWriter = nullptr;


	public:
		TSharedPtr<PCGExClusters::FCellConstraints> CellsConstraints;

		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;

		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
		virtual void OnRangeProcessingComplete() override;
				
		virtual void Cleanup() override;
	};
}
