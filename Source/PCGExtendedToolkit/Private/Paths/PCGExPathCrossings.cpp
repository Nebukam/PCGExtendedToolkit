// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathCrossings.h"
#include "PCGExMath.h"
#include "Data/Blending/PCGExUnionBlender.h"


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

bool FPCGExPathCrossingsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathCrossings)

	if (Settings->IntersectionDetails.bWriteCrossing) { PCGEX_VALIDATE_NAME(Settings->IntersectionDetails.CrossingAttributeName) }
	if (Settings->bWriteAlpha) { PCGEX_VALIDATE_NAME(Settings->CrossingAlphaAttributeName) }
	if (Settings->bWriteCrossDirection) { PCGEX_VALIDATE_NAME(Settings->CrossDirectionAttributeName) }

	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendOperation)

	GetInputFactories(Context, PCGExPaths::SourceCanCutFilters, Context->CanCutFilterFactories, PCGExFactories::PointFilters, false);
	GetInputFactories(Context, PCGExPaths::SourceCanBeCutFilters, Context->CanBeCutFilterFactories, PCGExFactories::PointFilters, false);

	Context->CrossingBlending = Settings->CrossingBlending;

	if (Settings->bOrientCrossing)
	{
		Context->CrossingBlending.PropertiesOverrides.bOverrideRotation = true;
		Context->CrossingBlending.PropertiesOverrides.RotationBlending = EPCGExDataBlendingType::None;
	}

	return true;
}

bool FPCGExPathCrossingsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathCrossingsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathCrossings)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{

		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))
		
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPathCrossings::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					Entry->InitializeOutput(Context, PCGExData::EInit::Forward); // TODO : This is no good as we'll be missing template attributes
					bHasInvalidInputs = true;

					if (Settings->bTagIfHasNoCrossings) { Entry->Tags->Add(Settings->HasNoCrossingsTag); }

					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExPathCrossings::FProcessor>>& NewBatch)
			{
				NewBatch->PrimaryOperation = Context->Blending;
				//NewBatch->SetPointsFilterData(&Context->FilterFactories);
				NewBatch->bRequiresWriteStep = Settings->bDoCrossBlending;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to intersect with."));
		}

	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExPathCrossings
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathCrossings::Process);

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }


		bClosedLoop = Context->ClosedLoop.IsClosedLoop(PointIO);
		bSelfIntersectionOnly = Settings->bSelfIntersectionOnly;
		Details = Settings->IntersectionDetails;
		Details.Init();

		CanCutFilterManager = MakeShared<PCGExPointFilter::FManager>(PointDataFacade);
		if (!CanCutFilterManager->Init(ExecutionContext, Context->CanCutFilterFactories)) { CanCutFilterManager.Reset(); }

		CanBeCutFilterManager = MakeShared<PCGExPointFilter::FManager>(PointDataFacade);
		if (!CanBeCutFilterManager->Init(ExecutionContext, Context->CanBeCutFilterFactories)) { CanBeCutFilterManager.Reset(); }

		// Build edges
		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		NumPoints = InPoints.Num();
		LastIndex = NumPoints - 1;

		PCGEx::InitArray(Positions, NumPoints);
		PCGEx::InitArray(Lengths, NumPoints);
		Edges.Init(nullptr, NumPoints);
		Crossings.Init(nullptr, NumPoints);

		CanCut.Init(true, NumPoints);
		CanBeCut.Init(true, NumPoints);

		FBox PointBounds = FBox(ForceInit);
		for (int i = 0; i < NumPoints; i++)
		{
			const FVector V = InPoints[i].Transform.GetLocation();
			Positions[i] = V;
			PointBounds += V;
		}

		EdgeOctree = MakeUnique<TEdgeOctree>(PointBounds.GetCenter(), PointBounds.GetExtent().Length() + (Details.Tolerance * 2));

		Blending = Cast<UPCGExSubPointsBlendOperation>(PrimaryOperation);
		Blending->bClosedLoop = bClosedLoop;
		if (Settings->bOrientCrossing) { Blending->bPreserveRotation = true; }

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, Preparation)
		Preparation->OnCompleteCallback =
			[&]()
			{
				CanCutFilterManager.Reset();
				CanBeCutFilterManager.Reset();

				if (!bClosedLoop)
				{
					CanBeCut[NumPoints - 1] = false;
					CanCut[NumPoints - 1] = false;
				}

				for (int i = 0; i < NumPoints; i++)
				{
					if (!CanCut[i]) { continue; } // !!
					EdgeOctree->AddElement(Edges[i].Get());
				}
			};

		Preparation->OnIterationRangeStartCallback =
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				PointDataFacade->Fetch(StartIndex, Count);
			};

		Preparation->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx)
		{
			const TSharedPtr<PCGExPaths::FPathEdge> Edge = MakeShared<PCGExPaths::FPathEdge>(Index, Index + 1 >= NumPoints ? 0 : Index + 1, Positions, Details.Tolerance);

			const double Length = FVector::DistSquared(Positions[Edge->Start], Positions[Edge->End]);
			Lengths[Index] = Length;

			if (Length > 0)
			{
				if (CanCutFilterManager) { CanCut[Index] = CanCutFilterManager->Test(Index); }
				if (CanBeCutFilterManager) { CanBeCut[Index] = CanBeCutFilterManager->Test(Index); }
			}

			Edges[Index] = Edge;
		};

		Preparation->StartIterations(NumPoints, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		Crossings[Index] = nullptr;

		if (!CanBeCut[Index]) { return; }
		if (!bClosedLoop && Index == LastIndex) { return; }

		const PCGExPaths::FPathEdge* Edge = Edges[Index].Get();

		int32 CurrentIOIndex = PointDataFacade->Source->IOIndex;
		const TArray<FVector>* P2 = &Positions;

		const TSharedPtr<FCrossing> NewCrossing = MakeShared<FCrossing>(Index);

		auto FindSplit = [&](const PCGExPaths::FPathEdge* E1, const PCGExPaths::FPathEdge* E2)
		{
			const FVector& A1 = Positions[E1->Start];
			const FVector& B1 = Positions[E1->End];
			const FVector& A2 = *(P2->GetData() + E2->Start);
			const FVector& B2 = *(P2->GetData() + E2->End);
			if (A1 == A2 || A1 == B2 || A2 == B1 || B2 == B1) { return; }

			const FVector CrossDir = (B2 - A2).GetSafeNormal();

			if (Details.bUseMinAngle || Details.bUseMaxAngle)
			{
				if (!Details.CheckDot(FMath::Abs(FVector::DotProduct((B1 - A1).GetSafeNormal(), CrossDir)))) { return; }
			}

			FVector A;
			FVector B;
			FMath::SegmentDistToSegment(
				A1, B1,
				A2, B2,
				A, B);

			if (A == A1 || A == B1 || B == A2 || B == B2) { return; }

			if (FVector::DistSquared(A, B) >= Details.ToleranceSquared) { return; }

			NewCrossing->Crossings.Add(PCGEx::H64(E2->Start, CurrentIOIndex));
			NewCrossing->Positions.Add(FMath::Lerp(A, B, 0.5));
			NewCrossing->CrossingDirections.Add(CrossDir);
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

		for (const TSharedPtr<PCGExPointsMT::FPointsProcessorBatchBase> Parent = ParentBatch.Pin();
		     const TSharedPtr<PCGExData::FFacade> Facade : Parent->ProcessorFacades)
		{
			const TSharedRef<FPointsProcessor>* OtherProcessorPtr = Parent->SubProcessorMap->Find(&Facade->Source.Get());
			if (!OtherProcessorPtr) { continue; }
			TSharedRef<FPointsProcessor> OtherProcessor = *OtherProcessorPtr;
			if (!Details.bEnableSelfIntersection && &OtherProcessor.Get() == this) { continue; }

			const TSharedRef<FProcessor> TypedProcessor = StaticCastSharedRef<FProcessor>(OtherProcessor);

			CurrentIOIndex = TypedProcessor->PointDataFacade->Source->IOIndex;
			P2 = &TypedProcessor->Positions;

			TypedProcessor->GetEdgeOctree()->FindElementsWithBoundsTest(
				Edge->FSBounds.GetBox(), [&](const PCGExPaths::FPathEdge* OtherEdge) { FindSplit(Edge, OtherEdge); });
		}

		if (!NewCrossing->Crossings.IsEmpty()) { Crossings[Index] = NewCrossing; }
	}

	void FProcessor::FixPoint(const int32 Index)
	{
		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		// TODO : Set crossing positions + blending
		const FCrossing* Crossing = Crossings[Index].Get();
		const PCGExPaths::FPathEdge* Edge = Edges[Index].Get();

		if (FlagWriter) { FlagWriter->GetMutable(Edge->OffsetedStart) = false; }
		if (AlphaWriter) { AlphaWriter->GetMutable(Edge->OffsetedStart) = Settings->DefaultAlpha; }
		if (CrossWriter) { CrossWriter->GetMutable(Edge->OffsetedStart) = Settings->DefaultCrossDirection; }

		if (!Crossing) { return; }

		const int32 NumCrossings = Crossing->Crossings.Num();
		const int32 CrossingStartIndex = Edge->OffsetedStart + 1;

		TArray<FPCGPoint>& OutPoints = PointIO->GetOut()->GetMutablePoints();
		PCGExPaths::FPathMetrics Metrics = PCGExPaths::FPathMetrics(Positions[Edge->Start]);

		TArray<int32> Order;
		PCGEx::ArrayOfIndices(Order, NumCrossings);
		Order.Sort([&](const int32 A, const int32 B) { return Crossing->Alphas[A] < Crossing->Alphas[B]; });

		for (int i = 0; i < NumCrossings; i++)
		{
			const int32 Idx = CrossingStartIndex + i;
			FPCGPoint& CrossingPt = OutPoints[Idx];

			const int32 OrderIdx = Order[i];
			const FVector& V = Crossing->Positions[OrderIdx];
			const FVector CrossDir = Crossing->CrossingDirections[OrderIdx];

			if (FlagWriter) { FlagWriter->GetMutable(Idx) = true; }
			if (AlphaWriter) { AlphaWriter->GetMutable(Idx) = Crossing->Alphas[OrderIdx]; }
			if (CrossWriter) { CrossWriter->GetMutable(Idx) = CrossDir; }

			if (Settings->bOrientCrossing)
			{
				CrossingPt.Transform.SetRotation(PCGExMath::MakeDirection(Settings->CrossingOrientAxis, CrossDir));
			}

			CrossingPt.Transform.SetLocation(V);
			Metrics.Add(V);
		}

		Metrics.Add(Positions[Edge->End]);

		const TArrayView<FPCGPoint> View = MakeArrayView(OutPoints.GetData() + CrossingStartIndex, NumCrossings);
		const int32 EndIndex = Index == LastIndex ? 0 : CrossingStartIndex + NumCrossings;
		Blending->ProcessSubPoints(PointIO->GetOutPointRef(CrossingStartIndex - 1), PointIO->GetOutPointRef(EndIndex), View, Metrics, CrossingStartIndex);
	}

	void FProcessor::CrossBlendPoint(const int32 Index)
	{
		const FCrossing* Crossing = Crossings[Index].Get();
		const PCGExPaths::FPathEdge* Edge = Edges[Index].Get();

		const int32 NumCrossings = Crossing->Crossings.Num();

		// Sorting again, ugh
		TArray<int32> Order;
		PCGEx::ArrayOfIndices(Order, NumCrossings);
		Order.Sort([&](const int32 A, const int32 B) { return Crossing->Alphas[A] < Crossing->Alphas[B]; });

		const TUniquePtr<PCGExData::FUnionData> Union = MakeUnique<PCGExData::FUnionData>();
		for (int i = 0; i < NumCrossings; i++)
		{
			uint32 PtIdx;
			uint32 IOIdx;
			PCGEx::H64(Crossing->Crossings[Order[i]], PtIdx, IOIdx);

			const int32 SecondIndex = PtIdx + 1 >= static_cast<uint32>(Context->MainPoints->Pairs[IOIdx]->GetNum(PCGExData::ESource::In)) ? 0 : PtIdx + 1;

			Union->Reset();
			Union->Add(IOIdx, PtIdx);
			Union->Add(IOIdx, SecondIndex);
			UnionBlender->SoftMergeSingle(Edge->OffsetedStart + i + 1, Union.Get(), Settings->CrossingBlendingDistance);
		}
	}

	void FProcessor::OnSearchComplete()
	{
		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		int32 NumPointsFinal = 0;

		for (int i = 0; i < NumPoints; i++)
		{
			NumPointsFinal++;

			const FCrossing* Crossing = Crossings[i].Get();
			if (!Crossing) { continue; }

			NumPointsFinal += Crossing->Crossings.Num();
		}

		PointIO->InitializeOutput(Context, PCGExData::EInit::NewOutput);

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		TArray<FPCGPoint>& OutPoints = PointIO->GetOut()->GetMutablePoints();
		UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;

		PCGEx::InitArray(OutPoints, NumPointsFinal);

		int32 Index = 0;
		for (int i = 0; i < NumPoints; i++)
		{
			Edges[i]->OffsetedStart = Index;

			const FPCGPoint& OriginalPoint = InPoints[i];
			OutPoints[Index] = OriginalPoint;
			Metadata->InitializeOnSet(OutPoints[Index++].MetadataEntry);

			const FCrossing* Crossing = Crossings[i].Get();
			if (!Crossing) { continue; }

			for (const uint64 Hash : Crossing->Crossings)
			{
				CrossIOIndices.Add(PCGEx::H64B(Hash));
				OutPoints[Index] = OriginalPoint;
				Metadata->InitializeOnSet(OutPoints[Index++].MetadataEntry);
			}
		}

		// Flag last so it doesn't get captured by blenders
		if (Settings->IntersectionDetails.bWriteCrossing)
		{
			FlagWriter = PointDataFacade->GetWritable(Settings->IntersectionDetails.CrossingAttributeName, false, true, true);
			ProtectedAttributes.Add(Settings->IntersectionDetails.CrossingAttributeName);
		}

		if (Settings->bWriteAlpha)
		{
			AlphaWriter = PointDataFacade->GetWritable<double>(Settings->CrossingAlphaAttributeName, Settings->DefaultAlpha, true, true);
			ProtectedAttributes.Add(Settings->CrossingAlphaAttributeName);
		}

		if (Settings->bWriteCrossDirection)
		{
			CrossWriter = PointDataFacade->GetWritable<FVector>(Settings->CrossDirectionAttributeName, Settings->DefaultCrossDirection, true, true);
			ProtectedAttributes.Add(Settings->CrossDirectionAttributeName);
		}

		Blending->PrepareForData(PointDataFacade, PointDataFacade, PCGExData::ESource::Out, &ProtectedAttributes);

		if (PointIO->GetIn()->GetPoints().Num() != PointIO->GetOut()->GetPoints().Num()) { if (Settings->bTagIfHasCrossing) { PointIO->Tags->Add(Settings->HasCrossingsTag); } }
		else { if (Settings->bTagIfHasNoCrossings) { PointIO->Tags->Add(Settings->HasNoCrossingsTag); } }

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, FixTask)
		FixTask->OnCompleteCallback = [&]() { PointDataFacade->Write(AsyncManager); };
		FixTask->OnIterationCallback = [&](const int32 FixIndex, const int32 Count, const int32 LoopIdx) { FixPoint(FixIndex); };
		FixTask->StartIterations(NumPoints, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, SearchTask)
		SearchTask->OnCompleteCallback = [&]() { OnSearchComplete(); };
		SearchTask->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx)
		{
			FPCGPoint Dummy;
			ProcessSinglePoint(Index, Dummy, LoopIdx, Count);
		};
		SearchTask->StartIterations(NumPoints, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}

	void FProcessor::Write()
	{
		UnionMetadata = MakeShared<PCGExData::FUnionMetadata>();
		UnionBlender = MakeShared<PCGExDataBlending::FUnionBlender>(&Settings->CrossingBlending, &Settings->CrossingCarryOver);
		for (const TSharedPtr<PCGExData::FPointIO> IO : Context->MainPoints->Pairs)
		{
			if (IO && CrossIOIndices.Contains(IO->IOIndex)) { UnionBlender->AddSource(Context->SubProcessorMap[IO.Get()]->PointDataFacade); }
		}

		UnionBlender->PrepareSoftMerge(PointDataFacade, UnionMetadata, &ProtectedAttributes);

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, CrossBlendTask)
		CrossBlendTask->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx)
		{
			if (!Crossings[Index]) { return; }
			CrossBlendPoint(Index);
		};
		CrossBlendTask->StartIterations(NumPoints, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
