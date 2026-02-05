// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExBuildCellDiagram.h"

#include "Data/PCGExData.h"
#include "Data/PCGPointArrayData.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Blenders/PCGExUnionBlender.h"
#include "Clusters/PCGExCluster.h"
#include "Clusters/PCGExClustersHelpers.h"
#include "Clusters/Artifacts/PCGExCell.h"
#include "Clusters/Artifacts/PCGExPlanarFaceEnumerator.h"
#include "Math/PCGExMathDistances.h"
#include "Graphs/PCGExGraph.h"
#include "Graphs/PCGExGraphBuilder.h"
#include "Helpers/PCGExMetaHelpers.h"
#include "Sampling/PCGExSamplingUnionData.h"

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

	PCGEX_FWD(CarryOverDetails)
	Context->CarryOverDetails.Init();

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

		// Get adjacency map (cached in enumerator)
		int32 WrapperFaceIndex = Enumerator->GetWrapperFaceIndex();
		CellAdjacencyMap = Enumerator->GetOrBuildAdjacencyMap(WrapperFaceIndex);

		// Build FaceIndex -> OutputIndex mapping
		for (int32 i = 0; i < NumCells; ++i)
		{
			if (ValidCells[i] && ValidCells[i]->FaceIndex >= 0)
			{
				FaceIndexToOutputIndex.Add(ValidCells[i]->FaceIndex, i);
			}
		}

		// Create output vertex data (cell centroids)
		TSharedPtr<PCGExData::FPointIO> CentroidIO = Context->MainPoints->Emplace_GetRef(VtxDataFacade->Source, PCGExData::EIOInit::New);
		CentroidIO->Tags->Reset();
		CentroidIO->IOIndex = BatchIndex;
		PCGExClusters::Helpers::CleanupClusterData(CentroidIO);

		PCGExPointArrayDataHelpers::SetNumPointsAllocated(CentroidIO->GetOut(), NumCells);

		CentroidFacade = MakeShared<PCGExData::FFacade>(CentroidIO.ToSharedRef());

		// Create and initialize union blender
		UnionBlender = MakeShared<PCGExBlending::FUnionBlender>(
			const_cast<FPCGExBlendingDetails*>(&Settings->BlendingDetails),
			&Context->CarryOverDetails,
			PCGExMath::GetNoneDistances());

		TArray<TSharedRef<PCGExData::FFacade>> BlendSources;
		BlendSources.Add(VtxDataFacade);
		UnionBlender->AddSources(BlendSources, &PCGExClusters::Labels::ProtectedClusterAttributes);

		if (!UnionBlender->Init(Context, CentroidFacade))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Failed to initialize blender for cell diagram."));
		}

		// Create attribute writers (after blender init so they aren't captured)
		if (Settings->bWriteArea)
		{
			AreaWriter = CentroidFacade->GetWritable<double>(Settings->AreaAttributeName, 0.0, true, PCGExData::EBufferInit::New);
		}

		if (Settings->bWriteCompactness)
		{
			CompactnessWriter = CentroidFacade->GetWritable<double>(Settings->CompactnessAttributeName, 0.0, true, PCGExData::EBufferInit::New);
		}

		if (Settings->bWriteNumNodes)
		{
			NumNodesWriter = CentroidFacade->GetWritable<int32>(Settings->NumNodesAttributeName, 0, true, PCGExData::EBufferInit::New);
		}

		StartParallelLoopForRange(NumCells);

		return true;
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		UPCGBasePointData* CentroidIO = CentroidFacade->GetOut();

		// Get value ranges for writing
		TPCGValueRange<FTransform> OutTransforms = CentroidIO->GetTransformValueRange();
		TPCGValueRange<FVector> OutBoundsMin = CentroidIO->GetBoundsMinValueRange();
		TPCGValueRange<FVector> OutBoundsMax = CentroidIO->GetBoundsMaxValueRange();

		// Write cell centroids and blend attributes using reset-able union data
		TArray<PCGExData::FWeightedPoint> WeightedPoints;
		TArray<PCGEx::FOpStats> Trackers;
		UnionBlender->InitTrackers(Trackers);

		const TSharedPtr<PCGExSampling::FSampingUnionData> Union = MakeShared<PCGExSampling::FSampingUnionData>();
		const int32 SourceIOIndex = VtxDataFacade->Source->IOIndex;

		PCGEX_SCOPE_LOOP(Index)
		{
			const TSharedPtr<PCGExClusters::FCell>& Cell = ValidCells[Index];
			if (!Cell) { continue; }

			// Set transform at centroid
			FTransform Transform = FTransform::Identity;
			Transform.SetLocation(Cell->Data.Centroid);
			OutTransforms[Index] = Transform;

			// Set bounds
			const FVector HalfExtent = Cell->Data.Bounds.GetExtent();
			OutBoundsMin[Index] = -HalfExtent;
			OutBoundsMax[Index] = HalfExtent;

			// Blend attributes from cell vertices using reset-able union data
			Union->Reset();
			Union->Reserve(1, Cell->Nodes.Num());
			for (const int32 NodeIdx : Cell->Nodes)
			{
				const int32 PointIdx = Cluster->GetNodePointIndex(NodeIdx);
				// Use equal weight (1.0) for all vertices in the cell
				Union->AddWeighted_Unsafe(PCGExData::FElement(PointIdx, SourceIOIndex), 1.0);
			}

			UnionBlender->ComputeWeights(Index, Union, WeightedPoints);
			UnionBlender->Blend(Index, WeightedPoints, Trackers);

			// Write cell-specific attributes
			if (AreaWriter) { AreaWriter->SetValue(Index, Cell->Data.Area); }
			if (CompactnessWriter) { CompactnessWriter->SetValue(Index, Cell->Data.Compactness); }
			if (NumNodesWriter) { NumNodesWriter->SetValue(Index, Cell->Nodes.Num()); }
		}
	}

	void FProcessor::OnRangeProcessingComplete()
	{
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
			return;
		}

		// Create graph and insert edges
		GraphBuilder = MakeShared<PCGExGraphs::FGraphBuilder>(CentroidFacade.ToSharedRef(), &Settings->GraphBuilderDetails);
		GraphBuilder->bInheritNodeData = false; // We created new points from scratch, don't inherit from input

		GraphBuilder->Graph = MakeShared<PCGExGraphs::FGraph>(CentroidFacade->GetNum(PCGExData::EIOSide::Out));
		GraphBuilder->Graph->InsertEdges(UniqueEdges, BatchIndex);

		// Set up edge output
		GraphBuilder->EdgesIO = Context->MainEdges;
		GraphBuilder->NodePointsTransforms = CentroidFacade->GetOut()->GetConstTransformValueRange();

		// Compile graph
		GraphBuilder->CompileAsync(TaskManager, true, nullptr);
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExBuildCellDiagramContext, UPCGExBuildCellDiagramSettings>::Cleanup();
		if (CellsConstraints) { CellsConstraints->Cleanup(); }
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
