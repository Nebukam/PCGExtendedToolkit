// Copyright Timothé Lapetite 2024
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
	PCGEX_PIN_PARAMS(PCGExPaths::SourceCanCutFilters, "Fiter which edges can 'cut' other edges. Leave empty so all edges are can cut other edges.", Normal, {})
	PCGEX_PIN_PARAMS(PCGExPaths::SourceCanBeCutFilters, "Fiter which edges can be 'cut' by other edges. Leave empty so all edges are can cut other edges.", Normal, {})
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

	if (Settings->bFlagCrossing) { PCGEX_VALIDATE_NAME(Settings->CrossingFlagAttributeName) }

	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInterpolate)
	Context->Blending->bClosedPath = Settings->bClosedPath;

	GetInputFactories(Context, PCGExPaths::SourceCanCutFilters, Context->CanCutFilterFactories, PCGExFactories::PointFilters, false);
	GetInputFactories(Context, PCGExPaths::SourceCanBeCutFilters, Context->CanBeCutFilterFactories, PCGExFactories::PointFilters, false);

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
				NewBatch->PrimaryOperation = Context->Blending;
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

		PCGEX_DELETE(CanCutFilterManager)
		PCGEX_DELETE(CanBeCutFilterManager)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathCrossings::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathCrossings)

		// Must be set before process for filters
		PointDataFacade->bSupportsDynamic = true;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalTypedContext = TypedContext;

		bClosedPath = Settings->bClosedPath;
		bSelfIntersectionOnly = Settings->bSelfIntersectionOnly;
		Details = Settings->IntersectionDetails;
		Details.Init();

		CanCutFilterManager = new PCGExPointFilter::TManager(PointDataFacade);
		if (!CanCutFilterManager->Init(Context, TypedContext->CanCutFilterFactories)) { PCGEX_DELETE(CanCutFilterManager) }

		CanBeCutFilterManager = new PCGExPointFilter::TManager(PointDataFacade);
		if (!CanBeCutFilterManager->Init(Context, TypedContext->CanBeCutFilterFactories)) { PCGEX_DELETE(CanBeCutFilterManager) }

		// Build edges
		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		NumPoints = InPoints.Num();
		LastIndex = NumPoints - 1;

		PCGEX_SET_NUM_UNINITIALIZED(Positions, NumPoints)
		PCGEX_SET_NUM_UNINITIALIZED(Lengths, NumPoints)
		PCGEX_SET_NUM_UNINITIALIZED(Edges, NumPoints)
		PCGEX_SET_NUM_UNINITIALIZED(Crossings, NumPoints)

		FBox PointBounds = FBox(ForceInit);
		for (int i = 0; i < NumPoints; i++)
		{
			const FVector V = InPoints[i].Transform.GetLocation();
			Positions[i] = V;
			PointBounds += V;
		}

		EdgeOctree = new TEdgeOctree(PointBounds.GetCenter(), PointBounds.GetExtent().Length() + (Details.Tolerance * 2));

		Blending = Cast<UPCGExSubPointsBlendOperation>(PrimaryOperation);

		PCGExMT::FTaskGroup* Preparation = AsyncManager->CreateGroup();

		Preparation->SetOnCompleteCallback(
			[&]()
			{
				PCGEX_DELETE(CanCutFilterManager)
				PCGEX_DELETE(CanBeCutFilterManager)

				if (!bClosedPath)
				{
					Edges.Last()->bCanBeCut = false;
					Edges.Last()->bCanCut = false;
				}

				for (int i = 0; i < NumPoints; i++)
				{
					if (!Edges[i]->bCanCut) { continue; } // !!
					EdgeOctree->AddElement(Edges[i]);
				}
			});

		Preparation->SetOnIterationRangeStartCallback(
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				PointDataFacade->Fetch(StartIndex, Count);
			});

		Preparation->StartRanges(
			[&](const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				PCGExPaths::FPathEdge* Edge = new PCGExPaths::FPathEdge(Index, Index + 1 >= NumPoints ? 0 : Index + 1, Positions, Details.Tolerance);

				const double Length = FVector::DistSquared(Positions[Edge->Start], Positions[Edge->End]);
				Lengths[Index] = Length;

				Edge->bCanBeCut = Length == 0 ? false : CanBeCutFilterManager ? CanBeCutFilterManager->Test(Index) : true;
				Edge->bCanCut = Length == 0 ? false : CanCutFilterManager ? CanCutFilterManager->Test(Index) : true;

				Edges[Index] = Edge;
			}, NumPoints, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		Crossings[Index] = nullptr;

		if (!bClosedPath && Index == LastIndex) { return; }

		const PCGExPaths::FPathEdge* Edge = Edges[Index];

		if (!Edge->bCanBeCut) { return; }

		int32 CurrentIOIndex = PointIO->IOIndex;
		const TArray<FVector>* P2 = &Positions;

		FCrossing* NewCrossing = new FCrossing(Index);

		auto FindSplit = [&](const PCGExPaths::FPathEdge* E1, const PCGExPaths::FPathEdge* E2)
		{
			const FVector& A1 = Positions[E1->Start];
			const FVector& B1 = Positions[E1->End];
			const FVector& A2 = *(P2->GetData() + E2->Start);
			const FVector& B2 = *(P2->GetData() + E2->End);
			if (A1 == A2 || A1 == B2 || A2 == B1 || B2 == B1) { return; }

			if (Details.bUseMinAngle || Details.bUseMaxAngle)
			{
				const double Dot = FVector::DotProduct((B1 - A1).GetSafeNormal(), (B2 - A2).GetSafeNormal());
				if (Dot < Details.MinDot || Dot > Details.MaxDot) { return; }
			}


			FVector A;
			FVector B;
			FMath::SegmentDistToSegment(
				A1, B1,
				A2, B2,
				A, B);

			if (FVector::DistSquared(A, B) >= Details.ToleranceSquared) { return; }

			NewCrossing->Crossings.Add(PCGEx::H64(E2->Start, CurrentIOIndex));
			NewCrossing->Positions.Add(FMath::Lerp(A, B, 0.5));
			NewCrossing->Alphas.Add(FVector::DistSquared(A1, A) / Lengths[E1->Start]);
		};

		// Find crossings
		if (bSelfIntersectionOnly)
		{
			EdgeOctree->FindElementsWithBoundsTest(
				Edge->FSBounds.GetBox(), [&](const PCGExPaths::FPathEdge* OtherEdge)
				{
					if (OtherEdge == Edge) { return; }
					FindSplit(Edge, OtherEdge);
				});
			return;
		}

		for (const PCGExData::FFacade* Facade : ParentBatch->ProcessorFacades)
		{
			FPointsProcessor** OtherProcessorPtr = ParentBatch->SubProcessorMap->Find(Facade->Source);
			if (!OtherProcessorPtr) { continue; }
			FPointsProcessor* OtherProcessor = *OtherProcessorPtr;
			if (!Details.bEnableSelfIntersection && OtherProcessor == this) { continue; }

			const FProcessor* TypedProcessor = static_cast<FProcessor*>(OtherProcessor);
			const TEdgeOctree* OtherOctree = TypedProcessor->GetEdgeOctree();

			CurrentIOIndex = TypedProcessor->PointIO->IOIndex;
			P2 = &TypedProcessor->Positions;

			OtherOctree->FindElementsWithBoundsTest(
				Edge->FSBounds.GetBox(), [&](const PCGExPaths::FPathEdge* OtherEdge) { FindSplit(Edge, OtherEdge); });
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
			if (FlagWriter) { FlagWriter->Values[Edge->OffsetedStart] = false; }
			return;
		}

		const int32 NumCrossings = Crossing->Crossings.Num();

		TArray<FPCGPoint>& OutPoints = PointIO->GetOut()->GetMutablePoints();
		PCGExMath::FPathMetricsSquared Metrics = PCGExMath::FPathMetricsSquared(Positions[Edge->Start]);

		TArray<int32> Order;
		PCGEx::ArrayOfIndices(Order, NumCrossings);
		Order.Sort([&](const int32 A, const int32 B) { return Crossing->Alphas[A] < Crossing->Alphas[B]; });

		// TODO : Blend by alpha using SubPoint blender
		for (int i = 0; i < NumCrossings; i++)
		{
			const int32 Index = Edge->OffsetedStart + i + 1;
			FPCGPoint& CrossingPt = OutPoints[Index];
			const FVector& V = Crossing->Positions[Order[i]];
			CrossingPt.Transform.SetLocation(V);
			Metrics.Add(V);
			if (FlagWriter) { FlagWriter->Values[Index] = true; }
		}

		Metrics.Add(Positions[Edge->End]);

		TArrayView<FPCGPoint> View = MakeArrayView(OutPoints.GetData() + Edge->OffsetedStart, NumCrossings + 1);
		const int32 EndIndex = Iteration == LastIndex ? 0 : Edge->OffsetedStart + NumCrossings + 1;
		Blending->ProcessSubPoints(PointIO->GetOutPointRef(Edge->OffsetedStart), PointIO->GetOutPointRef(EndIndex), View, Metrics);
	}

	void FProcessor::OnSearchComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathCrossings)

		// TODO : Find the final number of points we require
		int32 NumPointsFinal = 0;

		for (int i = 0; i < NumPoints; i++)
		{
			NumPointsFinal++;

			const FCrossing* Crossing = Crossings[i];
			if (!Crossing) { continue; }

			NumPointsFinal += Crossing->Crossings.Num();
		}

		PointIO->InitializeOutput(PCGExData::EInit::NewOutput);

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		TArray<FPCGPoint>& OutPoints = PointIO->GetOut()->GetMutablePoints();
		UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;

		PCGEX_SET_NUM_UNINITIALIZED(OutPoints, NumPointsFinal)

		TSet<int32> IOIndices;

		int32 Index = 0;
		for (int i = 0; i < NumPoints; i++)
		{
			Edges[i]->OffsetedStart = Index;

			const FPCGPoint& OriginalPoint = InPoints[i];
			OutPoints[Index] = OriginalPoint;
			Metadata->InitializeOnSet(OutPoints[Index++].MetadataEntry);

			const FCrossing* Crossing = Crossings[i];
			if (!Crossing) { continue; }

			for (const uint64 Hash : Crossing->Crossings)
			{
				IOIndices.Add(PCGEx::H64B(Hash));
				OutPoints[Index] = OriginalPoint;
				Metadata->InitializeOnSet(OutPoints[Index++].MetadataEntry);
			}
		}

		if (Settings->bFlagCrossing) { FlagWriter = PointDataFacade->GetWriter(Settings->CrossingFlagAttributeName, false, true, true); }
		Blending->PrepareForData(PointDataFacade, PointDataFacade, PCGExData::ESource::Out);

		StartParallelLoopForRange(NumPoints);
	}

	void FProcessor::CompleteWork()
	{
		PCGExMT::FTaskGroup* SearchTask = AsyncManagerPtr->CreateGroup();
		SearchTask->SetOnCompleteCallback([&]() { OnSearchComplete(); });
		SearchTask->StartRanges(
			[&](const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				FPCGPoint Dummy;
				ProcessSinglePoint(Index, Dummy, LoopIdx, Count);
			},
			NumPoints, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}

	void FProcessor::Write()
	{
		FPointsProcessor::Write();
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
