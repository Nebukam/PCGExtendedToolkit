// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExDataForward.h"

#include "Geometry/PCGExGeo.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Topology/PCGExTopology.h"

#include "PCGExPathfindingFindAllCells.generated.h"

namespace PCGExFindAllCells
{
	class FProcessor;
}


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="pathfinding/contours/find-all-cells"))
class UPCGExFindAllCellsSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindAllCells, "Pathfinding : Find All Cells", "Attempts to find the contours of all cluster cells.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorPathfinding; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExCellConstraintsDetails Constraints = FPCGExCellConstraintsDetails(true);

	/** Cell artifacts. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExCellArtifactsDetails Artifacts;

	/** Output a filtered set of points containing only seeds that generated a valid path */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	//bool bOutputSeeds = false;

	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionDetails ProjectionDetails;

	/** Whether or not to search for closest node using an octree. Depending on your dataset, enabling this may be either much faster, or much slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bUseOctreeSearch = false;

private:
	friend class FPCGExFindAllCellsElement;
};

struct FPCGExFindAllCellsContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExFindAllCellsElement;
	friend class FPCGExCreateBridgeTask;

	FPCGExCellArtifactsDetails Artifacts;

	TSharedPtr<PCGExTopology::FHoles> Holes;
	TSharedPtr<PCGExData::FFacade> HolesFacade;

	TSharedPtr<PCGExData::FPointIOCollection> Paths;
	TSharedPtr<PCGExData::FPointIO> Seeds;

	mutable FRWLock SeedOutputLock;
};

class FPCGExFindAllCellsElement final : public FPCGExEdgesProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(FindAllCells)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExFindAllCells
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExFindAllCellsContext, UPCGExFindAllCellsSettings>
	{
		int32 NumAttempts = 0;
		int32 LastBinary = -1;
		int32 OutputPathsNum = 0;

	protected:
		TSharedPtr<PCGExTopology::FHoles> Holes;
		bool bBuildExpandedNodes = false;
		TSharedPtr<PCGExTopology::FCell> WrapperCell;

	public:
		TSharedPtr<PCGExTopology::FCellConstraints> CellsConstraints;

		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessEdges(const PCGExMT::FScope& Scope) override;
		bool FindCell(const PCGExCluster::FNode& Node, const PCGExGraph::FEdge& Edge, const bool bSkipBinary = true);
		void ProcessCell(const TSharedPtr<PCGExTopology::FCell>& InCell);
		void EnsureRoamingClosedLoopProcessing();
		virtual void OnEdgesProcessingComplete() override;
		virtual void CompleteWork() override;
		virtual void Cleanup() override;
	};
}
