// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Clusters/Artifacts/PCGExCellDetails.h"
#include "Core/PCGExClustersProcessor.h"

#include "PCGExPathfindingFindAllCells.generated.h"

namespace PCGExClusters
{
	class FCellConstraints;
	class FHoles;
}

namespace PCGExFindAllCells
{
	class FProcessor;
}

namespace PCGExMT
{
	template <typename T>
	class TScopedArray;
}


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="pathfinding/contours/find-all-cells"))
class UPCGExFindAllCellsSettings : public UPCGExClustersProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindAllCells, "Pathfinding : Find All Cells", "Attempts to find the contours of all cluster cells.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(Pathfinding); }
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

struct FPCGExFindAllCellsContext final : FPCGExClustersProcessorContext
{
	friend class FPCGExFindAllCellsElement;
	friend class FPCGExCreateBridgeTask;

	FPCGExCellArtifactsDetails Artifacts;

	TSharedPtr<PCGExClusters::FHoles> Holes;
	TSharedPtr<PCGExData::FFacade> HolesFacade;

	TSharedPtr<PCGExData::FPointIOCollection> OutputPaths;
	TSharedPtr<PCGExData::FPointIO> Seeds;

	mutable FRWLock SeedOutputLock;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExFindAllCellsElement final : public FPCGExClustersProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(FindAllCells)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExFindAllCells
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExFindAllCellsContext, UPCGExFindAllCellsSettings>
	{
		int32 NumAttempts = 0;
		int32 LastBinary = -1;

	protected:
		TSharedPtr<PCGExClusters::FHoles> Holes;
		bool bBuildExpandedNodes = false;
		TSharedPtr<PCGExClusters::FCell> WrapperCell;

		TSharedPtr<PCGExMT::TScopedArray<TSharedPtr<PCGExClusters::FCell>>> ScopedValidCells;
		TArray<TSharedPtr<PCGExClusters::FCell>> ValidCells;
		TArray<TSharedPtr<PCGExData::FPointIO>> CellsIO;

	public:
		TSharedPtr<PCGExClusters::FCellConstraints> CellsConstraints;

		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		virtual void PrepareLoopScopesForEdges(const TArray<PCGExMT::FScope>& Loops) override;
		virtual void ProcessEdges(const PCGExMT::FScope& Scope) override;
		bool FindCell(const PCGExClusters::FNode& Node, const PCGExGraphs::FEdge& Edge, TArray<TSharedPtr<PCGExClusters::FCell>>& Scope, const bool bSkipBinary = true);
		void ProcessCell(const TSharedPtr<PCGExClusters::FCell>& InCell, const TSharedPtr<PCGExData::FPointIO>& PathIO);
		void EnsureRoamingClosedLoopProcessing();

		virtual void OnEdgesProcessingComplete() override;
		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;

		virtual void Cleanup() override;
	};
}
