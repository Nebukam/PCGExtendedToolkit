﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathCrossings.h"
#include "PCGExMath.h"
#include "PCGExRandom.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExPathCrossingsElement"
#define PCGEX_NAMESPACE PathCrossings

TArray<FPCGPinProperties> UPCGExPathCrossingsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	// TODO : Filter pin
	return PinProperties;
}

PCGExData::EInit UPCGExPathCrossingsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(PathCrossings)

FPCGExPathCrossingsContext::~FPCGExPathCrossingsContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExPathCrossingsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathCrossings)

	if (Settings->bFlagSubPoints) { PCGEX_VALIDATE_NAME(Settings->FlagName) }

	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInterpolate)
	Context->Blending->bClosedPath = Settings->bClosedPath;

	return true;
}

bool FPCGExPathCrossingsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathCrossingsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathCrossings)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bHasInvalidInputs = false;
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPathCrossings::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 2)
				{
					Entry->InitializeOutput(PCGExData::EInit::Forward); // TODO : This is no good as we'll be missing template attributes
					bHasInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExPathCrossings::FProcessor>* NewBatch)
			{
				//NewBatch->SetPointsFilterData(&Context->FilterFactories);
				NewBatch->bRequiresWriteStep = true;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any paths to intersect with."));
			return true;
		}

		if (bHasInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExPathCrossings
{
	FProcessor::~FProcessor()
	{
		Positions.Empty();
		PCGEX_DELETE_TARRAY(Edges)
		PCGEX_DELETE_TARRAY(Crossings)
		PCGEX_DELETE(EdgeOctree)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathCrossings::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathCrossings)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalTypedContext = TypedContext;
		PointDataFacade->bSupportsDynamic = true;

		bClosedPath = Settings->bClosedPath;
		bSelfIntersectionOnly = Settings->bSelfIntersectionOnly;
		Details = Settings->IntersectionDetails;

		// Build edges
		const int32 NumPoints = PointIO->GetNum();
		const int32 NumEdges = bClosedPath ? NumPoints : NumPoints - 1;
		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();

		LastIndex = NumPoints - 1;

		PCGEX_SET_NUM_UNINITIALIZED(Positions, NumPoints)
		PCGEX_SET_NUM_UNINITIALIZED(Edges, NumEdges)

		FBox PointBounds = FBox(ForceInit);
		for (int i = 0; i < NumPoints; i++)
		{
			const FVector V = InPoints[i].Transform.GetLocation();
			Positions[i] = V;
			PointBounds += V;
		}

		EdgeOctree = new TEdgeOctree(PointBounds.GetCenter(), PointBounds.GetExtent().Length() + (Details.Tolerance * 2));

		Blending = Cast<UPCGExSubPointsBlendOperation>(PrimaryOperation);

		for (int i = 0; i < NumEdges; i++)
		{
			PCGExPaths::FPathEdge* Edge = new PCGExPaths::FPathEdge(i, i + 1 > NumPoints ? 0 : i + 1, Positions, Details.Tolerance);
			Edges[i] = Edge;
		}

		PCGExMT::FTaskGroup* Preparation = AsyncManager->CreateGroup();

		Preparation->SetOnCompleteCallback(
			[&]()
			{
				for (int i = 0; i < NumEdges; i++)
				{
					if (!Edges[i]->bCutter) { continue; } // !!
					EdgeOctree->AddElement(Edges[i]);
				}
				StartParallelLoopForPoints();
			});

		Preparation->SetOnIterationRangeStartCallback(
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				PointDataFacade->Fetch(StartIndex, Count);
			});

		Preparation->StartRanges(
			[&](const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				PCGExPaths::FPathEdge* Edge = new PCGExPaths::FPathEdge(Index, Index + 1 > NumPoints ? 0 : Index + 1, Positions, Details.Tolerance);
				Edge->bCuttable = true; // TODO : Filter driven
				Edge->bCutter = true;   // TODO : Filter driven
				Edges[Index] = Edge;
			}, NumPoints, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		if (!bClosedPath && Index == LastIndex) { return; }

		const PCGExPaths::FPathEdge* Edge = Edges[Index];

		if (!Edge->bCuttable) { return; }

		// Find crossings
		if (bSelfIntersectionOnly)
		{
			EdgeOctree->FindElementsWithBoundsTest(
				Edge->FSBounds.GetBox(), [&](const PCGExPaths::FPathEdge* OtherEdge)
				{
					if (OtherEdge == Edge) { return; }
					// TODO : Find crossings
				});
			return;
		}

		FCrossing* NewCrossing = new FCrossing(Index);

		for (const PCGExData::FFacade* Facade : ParentBatch->ProcessorFacades)
		{
			PCGExPointsMT::FPointsProcessor** OtherProcessorPtr = ParentBatch->SubProcessorMap->Find(Facade->Source);
			if (!OtherProcessorPtr) { continue; }
			PCGExPointsMT::FPointsProcessor* OtherProcessor = *OtherProcessorPtr;
			if (!Details.bEnableSelfIntersection && OtherProcessor == this) { continue; }

			const FProcessor* TypedProcessor = static_cast<FProcessor*>(OtherProcessor);
			const TEdgeOctree* OtherOctree = TypedProcessor->GetEdgeOctree();

			OtherOctree->FindElementsWithBoundsTest(
				Edge->FSBounds.GetBox(), [&](PCGExPaths::FPathEdge* OtherEdge)
				{
					// TODO : Find crossings					
				});
		}

		if (NewCrossing->Crossings.IsEmpty()) { PCGEX_DELETE(NewCrossing) }
		Crossings[Index] = NewCrossing;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		// TODO : Set crossing positions + blending
		const FCrossing* Crossing = Crossings[Iteration];
		const PCGExPaths::FPathEdge* Edge = Edges[Iteration];
		if (!Crossing)
		{
			// TODO : Flag as false for IsCrossing etc
			return;
		}

		const int32 NumCrossings = Crossing->Crossings.Num();

		TArray<FPCGPoint>& OutPoints = PointIO->GetOut()->GetMutablePoints();
		PCGExMath::FPathMetricsSquared Metrics = PCGExMath::FPathMetricsSquared(Positions[Edge->Start]);

		// TODO : Compute alphas
		// TODO : Sort crossings by alpha
		// TODO : Blend by alpha using SubPoint blender
		for (int i = 0; i < NumCrossings; i++)
		{
			const int32 Index = Edge->OffsetedStart + i + 1;
			FPCGPoint& CrossingPt = OutPoints[Index];
			if (FlagWriter) { FlagWriter->Values[Index] = true; }
		}

		Metrics.Add(Positions[Edge->End]);

		TArrayView<FPCGPoint> View = MakeArrayView(OutPoints.GetData() + Edge->OffsetedStart, NumCrossings + 1);
		Blending->ProcessSubPoints(PointIO->GetOutPointRef(Edge->OffsetedStart), PointIO->GetOutPointRef(Edge->OffsetedStart + NumCrossings + 1), View, Metrics);
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathCrossings)

		// TODO : Find the final number of points we require
		const int32 NumPoints = PointIO->GetNum();
		int32 NumPointsFinal = 0;

		for (int i = 0; i < NumPoints; i++)
		{
			Edges[i]->OffsetedStart = NumPointsFinal++;

			const FCrossing* Crossing = Crossings[i];
			if (!Crossing) { continue; }

			NumPointsFinal += Crossing->Crossings.Num();
		}

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		TArray<FPCGPoint>& OutPoints = PointIO->GetOut()->GetMutablePoints();
		UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;

		PCGEX_SET_NUM_UNINITIALIZED(OutPoints, NumPointsFinal)

		TSet<int32> IOIndices;

		int32 Index = 0;
		for (int i = 0; i < NumPoints; i++)
		{
			const FPCGPoint& OriginalPoint = InPoints[i];
			OutPoints[Index++] = OriginalPoint;
			Metadata->InitializeOnSet(OutPoints[NumPointsFinal].MetadataEntry);

			const FCrossing* Crossing = Crossings[i];
			if (!Crossing) { continue; }

			for (const uint64 Hash : Crossing->Crossings)
			{
				IOIndices.Add(PCGEx::H64B(Hash));
				OutPoints[Index++] = OriginalPoint;
				Metadata->InitializeOnSet(OutPoints[NumPointsFinal].MetadataEntry);
			}
		}

		Blending->PrepareForData(PointDataFacade, PointDataFacade, PCGExData::ESource::Out);

		StartParallelLoopForRange(NumPoints);
	}

	void FProcessor::Write()
	{
		FPointsProcessor::Write();
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE