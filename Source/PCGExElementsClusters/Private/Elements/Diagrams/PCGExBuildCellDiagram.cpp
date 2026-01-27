// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/Diagrams/PCGExBuildCellDiagram.h"

#include "Data/PCGExData.h"
#include "Data/PCGPointArrayData.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Clusters/PCGExCluster.h"
#include "Clusters/PCGExClustersHelpers.h"
#include "Clusters/Artifacts/PCGExCell.h"
#include "Clusters/Artifacts/PCGExPlanarFaceEnumerator.h"
#include "Graphs/PCGExGraph.h"
#include "Graphs/PCGExGraphBuilder.h"
#include "Helpers/PCGExMetaHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExBuildCellDiagram"
#define PCGEX_NAMESPACE BuildCellDiagram

TArray<FPCGPinProperties> UPCGExBuildCellDiagramSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGExClusters::Labels::SourceHolesLabel, "Omit cells that contain any points from this dataset", Normal)
	return PinProperties;
}

PCGExData::EIOInit UPCGExBuildCellDiagramSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::NoInit; }
PCGExData::EIOInit UPCGExBuildCellDiagramSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NoInit; }

PCGEX_INITIALIZE_ELEMENT(BuildCellDiagram)
PCGEX_ELEMENT_BATCH_EDGE_IMPL(BuildCellDiagram)

bool FPCGExBuildCellDiagramElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClustersProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildCellDiagram)

	// Validate attribute names
	if (Settings->bWriteArea) { PCGEX_VALIDATE_NAME_C(Context, Settings->AreaAttributeName); }
	if (Settings->bWriteCompactness) { PCGEX_VALIDATE_NAME_C(Context, Settings->CompactnessAttributeName); }
	if (Settings->bWriteNumNodes) { PCGEX_VALIDATE_NAME_C(Context, Settings->NumNodesAttributeName); }

	Context->HolesFacade = PCGExData::TryGetSingleFacade(Context, PCGExClusters::Labels::SourceHolesLabel, false, false);
	if (Context->HolesFacade && Settings->ProjectionDetails.Method == EPCGExProjectionMethod::Normal)
	{
		Context->Holes = MakeShared<PCGExClusters::FProjectedPointSet>(Context, Context->HolesFacade.ToSharedRef(), Settings->ProjectionDetails);
		Context->Holes->EnsureProjected();
	}

	return true;
}

bool FPCGExBuildCellDiagramElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildCellDiagramElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildCellDiagram)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters([](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; }, [&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
		{
			NewBatch->bSkipCompletion = true;
			NewBatch->SetProjectionDetails(Settings->ProjectionDetails);
		}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}


namespace PCGExBuildCellDiagram
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBuildCellDiagram::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }

		if (Context->HolesFacade)
		{
			Holes = Context->Holes ? Context->Holes : MakeShared<PCGExClusters::FProjectedPointSet>(Context, Context->HolesFacade.ToSharedRef(), ProjectionDetails);
			if (Holes) { Holes->EnsureProjected(); }
		}

		// Set up cell constraints
		CellsConstraints = MakeShared<PCGExClusters::FCellConstraints>(Settings->Constraints);
		CellsConstraints->Reserve(Cluster->Edges->Num());
		CellsConstraints->Holes = Holes;

		// Build or get the shared enumerator
		TSharedPtr<PCGExClusters::FPlanarFaceEnumerator> Enumerator = CellsConstraints->GetOrBuildEnumerator(Cluster.ToSharedRef(), ProjectionDetails);

		// Enumerate all cells (wrapper is omitted by default for graph)
		Enumerator->EnumerateAllFaces(ValidCells, CellsConstraints.ToSharedRef(), nullptr, true);

		const int32 NumCells = ValidCells.Num();
		if (NumCells < 2)
		{
			// Need at least 2 cells to form a graph
			bIsProcessorValid = false;
			return true;
		}

		// Build adjacency map
		int32 WrapperFaceIndex = Enumerator->GetWrapperFaceIndex();
		CellAdjacencyMap = Enumerator->BuildCellAdjacencyMap(WrapperFaceIndex);

		// Build FaceIndex -> OutputIndex mapping
		for (int32 i = 0; i < NumCells; ++i)
		{
			if (ValidCells[i] && ValidCells[i]->FaceIndex >= 0)
			{
				FaceIndexToOutputIndex.Add(ValidCells[i]->FaceIndex, i);
			}
		}

		// Create output vertex data (cell centroids)
		TSharedPtr<PCGExData::FPointIO> VtxIO = Context->MainPoints->Emplace_GetRef(VtxDataFacade->Source, PCGExData::EIOInit::New);
		VtxIO->Tags->Reset();
		VtxIO->IOIndex = BatchIndex;
		PCGExClusters::Helpers::CleanupClusterData(VtxIO);

		UPCGBasePointData* VtxPointData = VtxIO->GetOut();
		PCGExPointArrayDataHelpers::SetNumPointsAllocated(VtxPointData, NumCells);

		// Get value ranges for writing
		TPCGValueRange<FTransform> OutTransforms = VtxPointData->GetTransformValueRange();
		TPCGValueRange<FVector> OutBoundsMin = VtxPointData->GetBoundsMinValueRange();
		TPCGValueRange<FVector> OutBoundsMax = VtxPointData->GetBoundsMaxValueRange();

		PCGEX_MAKE_SHARED(VtxFacade, PCGExData::FFacade, VtxIO.ToSharedRef())

		// Create attribute writers
		TSharedPtr<PCGExData::TBuffer<double>> AreaWriter = Settings->bWriteArea ?
			VtxFacade->GetWritable<double>(PCGExMetaHelpers::MakeElementIdentifier(Settings->AreaAttributeName), 0.0, true, PCGExData::EBufferInit::New) : nullptr;
		TSharedPtr<PCGExData::TBuffer<double>> CompactnessWriter = Settings->bWriteCompactness ?
			VtxFacade->GetWritable<double>(PCGExMetaHelpers::MakeElementIdentifier(Settings->CompactnessAttributeName), 0.0, true, PCGExData::EBufferInit::New) : nullptr;
		TSharedPtr<PCGExData::TBuffer<int32>> NumNodesWriter = Settings->bWriteNumNodes ?
			VtxFacade->GetWritable<int32>(PCGExMetaHelpers::MakeElementIdentifier(Settings->NumNodesAttributeName), 0, true, PCGExData::EBufferInit::New) : nullptr;

		// Write cell centroids as points
		for (int32 i = 0; i < NumCells; ++i)
		{
			const TSharedPtr<PCGExClusters::FCell>& Cell = ValidCells[i];
			if (!Cell) { continue; }

			// Set transform at centroid
			FTransform Transform = FTransform::Identity;
			Transform.SetLocation(Cell->Data.Centroid);
			OutTransforms[i] = Transform;

			// Set bounds
			const FVector HalfExtent = Cell->Data.Bounds.GetExtent();
			OutBoundsMin[i] = -HalfExtent;
			OutBoundsMax[i] = HalfExtent;

			// Write attributes
			if (AreaWriter) { AreaWriter->SetValue(i, Cell->Data.Area); }
			if (CompactnessWriter) { CompactnessWriter->SetValue(i, Cell->Data.Compactness); }
			if (NumNodesWriter) { NumNodesWriter->SetValue(i, Cell->Nodes.Num()); }
		}

		// Build edges from adjacency
		TSet<uint64> UniqueEdges;
		for (const TSharedPtr<PCGExClusters::FCell>& Cell : ValidCells)
		{
			if (!Cell || Cell->FaceIndex < 0) { continue; }

			const int32* PointAPtr = FaceIndexToOutputIndex.Find(Cell->FaceIndex);
			if (!PointAPtr) { continue; }
			const int32 PointA = *PointAPtr;

			if (const TSet<int32>* Adjacent = CellAdjacencyMap.Find(Cell->FaceIndex))
			{
				for (int32 AdjFace : *Adjacent)
				{
					const int32* PointBPtr = FaceIndexToOutputIndex.Find(AdjFace);
					if (!PointBPtr) { continue; }
					const int32 PointB = *PointBPtr;

					// Use H64U to ensure unique edges (A,B) == (B,A)
					uint64 Hash = PCGEx::H64U(PointA, PointB);
					UniqueEdges.Add(Hash);
				}
			}
		}

		if (UniqueEdges.IsEmpty())
		{
			bIsProcessorValid = false;
			return true;
		}

		// Create graph and insert edges
		GraphBuilder = MakeShared<PCGExGraphs::FGraphBuilder>(VtxFacade.ToSharedRef(), &Settings->GraphBuilderDetails);
		GraphBuilder->bInheritNodeData = false; // We created new points from scratch, don't inherit from input
		GraphBuilder->Graph = MakeShared<PCGExGraphs::FGraph>(NumCells);
		GraphBuilder->Graph->InsertEdges(UniqueEdges, BatchIndex);

		// Set up edge output
		GraphBuilder->EdgesIO = Context->MainEdges;
		GraphBuilder->NodePointsTransforms = VtxPointData->GetConstTransformValueRange();

		// Compile graph
		GraphBuilder->CompileAsync(TaskManager, true, nullptr);

		// Write vertex facade
		VtxFacade->WriteFastest(TaskManager);

		return true;
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExBuildCellDiagramContext, UPCGExBuildCellDiagramSettings>::Cleanup();
		if (CellsConstraints) { CellsConstraints->Cleanup(); }
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
