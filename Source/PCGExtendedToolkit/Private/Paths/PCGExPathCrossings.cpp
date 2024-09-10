// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathCrossings.h"
#include "PCGExMath.h"
#include "PCGExRandom.h"
#include "Data/Blending/PCGExCompoundBlender.h"
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

	if (Settings->IntersectionDetails.bWriteCrossing) { PCGEX_VALIDATE_NAME(Settings->IntersectionDetails.CrossingAttributeName) }
	if (Settings->bWriteAlpha) { PCGEX_VALIDATE_NAME(Settings->CrossingAlphaAttributeName) }
	if (Settings->bWriteCrossDirection) { PCGEX_VALIDATE_NAME(Settings->CrossDirectionAttributeName) }

	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInterpolate)
	Context->Blending->bClosedPath = Settings->bClosedPath;

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

					if (Settings->bTagIfHasNoCrossings) { Entry->Tags->Add(Settings->HasNoCrossingsTag); }

					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExPathCrossings::FProcessor>* NewBatch)
			{
				NewBatch->PrimaryOperation = Context->Blending;
				//NewBatch->SetPointsFilterData(&Context->FilterFactories);
				NewBatch->bRequiresWriteStep = Settings->bDoCrossBlending;
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

		PCGEX_DELETE(CompoundList)
		PCGEX_DELETE(CompoundBlender)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathCrossings::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathCrossings)

		// Must be set before process for filters
		PointDataFacade->bSupportsDynamic = true;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalSettings = Settings;
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
		PCGEX_SET_NUM_NULLPTR(Crossings, NumPoints)

		CanCut.Init(true, NumPoints);
		CanBeCut.Init(true, NumPoints);

		FBox PointBounds = FBox(ForceInit);
		for (int i = 0; i < NumPoints; i++)
		{
			const FVector V = InPoints[i].Transform.GetLocation();
			Positions[i] = V;
			PointBounds += V;
		}

		EdgeOctree = new TEdgeOctree(PointBounds.GetCenter(), PointBounds.GetExtent().Length() + (Details.Tolerance * 2));

		Blending = Cast<UPCGExSubPointsBlendOperation>(PrimaryOperation);
		if (Settings->bOrientCrossing) { Blending->bPreserveRotation = true; }

		PCGEX_ASYNC_GROUP(AsyncManagerPtr, Preparation)
		Preparation->SetOnCompleteCallback(
			[&]()
			{
				PCGEX_DELETE(CanCutFilterManager)
				PCGEX_DELETE(CanBeCutFilterManager)

				if (!bClosedPath)
				{
					CanBeCut[NumPoints - 1] = false;
					CanCut[NumPoints - 1] = false;
				}

				for (int i = 0; i < NumPoints; i++)
				{
					if (!CanCut[i]) { continue; } // !!
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

				if (Length > 0)
				{
					if (CanCutFilterManager) { CanCut[Index] = CanCutFilterManager->Test(Index); }
					if (CanBeCutFilterManager) { CanBeCut[Index] = CanBeCutFilterManager->Test(Index); }
				}

				Edges[Index] = Edge;
			}, NumPoints, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		Crossings[Index] = nullptr;

		if (!CanBeCut[Index]) { return; }
		if (!bClosedPath && Index == LastIndex) { return; }

		const PCGExPaths::FPathEdge* Edge = Edges[Index];

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

	void FProcessor::FixPoint(const int32 Index)
	{
		// TODO : Set crossing positions + blending
		const FCrossing* Crossing = Crossings[Index];
		const PCGExPaths::FPathEdge* Edge = Edges[Index];

		if (FlagWriter) { FlagWriter->Values[Edge->OffsetedStart] = false; }
		if (AlphaWriter) { AlphaWriter->Values[Edge->OffsetedStart] = LocalSettings->DefaultAlpha; }
		if (CrossWriter) { CrossWriter->Values[Edge->OffsetedStart] = LocalSettings->DefaultCrossDirection; }

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

			if (FlagWriter) { FlagWriter->Values[Idx] = true; }
			if (AlphaWriter) { AlphaWriter->Values[Idx] = Crossing->Alphas[OrderIdx]; }
			if (CrossWriter) { CrossWriter->Values[Idx] = CrossDir; }

			if (LocalSettings->bOrientCrossing)
			{
				switch (LocalSettings->CrossingOrientAxis)
				{
				case EPCGExAxis::Forward:
					CrossingPt.Transform.SetRotation(FRotationMatrix::MakeFromX(CrossDir).ToQuat());
					break;
				case EPCGExAxis::Backward:
					CrossingPt.Transform.SetRotation(FRotationMatrix::MakeFromX(CrossDir * -1).ToQuat());
					break;
				case EPCGExAxis::Right:
					CrossingPt.Transform.SetRotation(FRotationMatrix::MakeFromY(CrossDir).ToQuat());
					break;
				case EPCGExAxis::Left:
					CrossingPt.Transform.SetRotation(FRotationMatrix::MakeFromY(CrossDir * -1).ToQuat());
					break;
				case EPCGExAxis::Up:
					CrossingPt.Transform.SetRotation(FRotationMatrix::MakeFromZ(CrossDir).ToQuat());
					break;
				case EPCGExAxis::Down:
					CrossingPt.Transform.SetRotation(FRotationMatrix::MakeFromZ(CrossDir * -1).ToQuat());
					break;
				default: ;
				}
			}

			CrossingPt.Transform.SetLocation(V);
			Metrics.Add(V);
		}

		Metrics.Add(Positions[Edge->End]);

		TArrayView<FPCGPoint> View = MakeArrayView(OutPoints.GetData() + CrossingStartIndex, NumCrossings);
		const int32 EndIndex = Index == LastIndex ? 0 : CrossingStartIndex + NumCrossings;
		Blending->ProcessSubPoints(PointIO->GetOutPointRef(CrossingStartIndex - 1), PointIO->GetOutPointRef(EndIndex), View, Metrics, CrossingStartIndex);
	}

	void FProcessor::CrossBlendPoint(const int32 Index)
	{
		// TODO : Set crossing positions + blending
		const FCrossing* Crossing = Crossings[Index];
		const PCGExPaths::FPathEdge* Edge = Edges[Index];

		const int32 NumCrossings = Crossing->Crossings.Num();

		// Sorting again, ugh
		TArray<int32> Order;
		PCGEx::ArrayOfIndices(Order, NumCrossings);
		Order.Sort([&](const int32 A, const int32 B) { return Crossing->Alphas[A] < Crossing->Alphas[B]; });

		PCGExData::FIdxCompound* TempCompound = new PCGExData::FIdxCompound();
		for (int i = 0; i < NumCrossings; i++)
		{
			uint32 PtIdx;
			uint32 IOIdx;
			PCGEx::H64(Crossing->Crossings[Order[i]], PtIdx, IOIdx);

			const int32 SecondIndex = PtIdx + 1 >= static_cast<uint32>(LocalTypedContext->MainPoints->Pairs[IOIdx]->GetNum(PCGExData::ESource::In)) ? 0 : PtIdx + 1;

			TempCompound->Clear();
			TempCompound->Add(IOIdx, PtIdx);
			TempCompound->Add(IOIdx, SecondIndex);
			CompoundBlender->SoftMergeSingle(Edge->OffsetedStart + i + 1, TempCompound, LocalSettings->CrossingBlendingDistance);
		}
		PCGEX_DELETE(TempCompound)
	}

	void FProcessor::OnSearchComplete()
	{
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

		// Flag last so it doesn't get captured by blenders
		if (LocalSettings->IntersectionDetails.bWriteCrossing)
		{
			FlagWriter = PointDataFacade->GetWriter(LocalSettings->IntersectionDetails.CrossingAttributeName, false, true, true);
			ProtectedAttributes.Add(LocalSettings->IntersectionDetails.CrossingAttributeName);
		}

		if (LocalSettings->bWriteAlpha)
		{
			AlphaWriter = PointDataFacade->GetWriter<double>(LocalSettings->CrossingAlphaAttributeName, LocalSettings->DefaultAlpha, true, true);
			ProtectedAttributes.Add(LocalSettings->CrossingAlphaAttributeName);
		}

		if (LocalSettings->bWriteCrossDirection)
		{
			CrossWriter = PointDataFacade->GetWriter<FVector>(LocalSettings->CrossDirectionAttributeName, LocalSettings->DefaultCrossDirection, true, true);
			ProtectedAttributes.Add(LocalSettings->CrossDirectionAttributeName);
		}

		Blending->PrepareForData(PointDataFacade, PointDataFacade, PCGExData::ESource::Out, &ProtectedAttributes);

		if (LocalSettings->bDoCrossBlending)
		{
			CompoundList = new PCGExData::FIdxCompoundList();
			CompoundBlender = new PCGExDataBlending::FCompoundBlender(&LocalSettings->CrossingBlending, &LocalSettings->CrossingCarryOver);
			for (const PCGExData::FPointIO* IO : LocalTypedContext->MainPoints->Pairs)
			{
				if (IO && IOIndices.Contains(IO->IOIndex)) { CompoundBlender->AddSource(LocalTypedContext->SubProcessorMap[IO]->PointDataFacade); }
			}

			CompoundBlender->PrepareSoftMerge(PointDataFacade, CompoundList, &ProtectedAttributes);
		}

		if (PointIO->GetIn()->GetPoints().Num() != PointIO->GetOut()->GetPoints().Num()) { if (LocalSettings->bTagIfHasCrossing) { PointIO->Tags->Add(LocalSettings->HasCrossingsTag); } }
		else { if (LocalSettings->bTagIfHasNoCrossings) { PointIO->Tags->Add(LocalSettings->HasNoCrossingsTag); } }

		PCGEX_ASYNC_GROUP(AsyncManagerPtr, FixTask)
		FixTask->SetOnCompleteCallback([&]() { PointDataFacade->Write(AsyncManagerPtr, true); });
		FixTask->StartRanges(
			[&](const int32 FixIndex, const int32 Count, const int32 LoopIdx) { FixPoint(FixIndex); },
			NumPoints, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_ASYNC_GROUP(AsyncManagerPtr, SearchTask)
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
		if (LocalSettings->bDoCrossBlending)
		{
			PCGEX_ASYNC_GROUP(AsyncManagerPtr, CrossBlendTask)
			CrossBlendTask->StartRanges(
				[&](const int32 Index, const int32 Count, const int32 LoopIdx)
				{
					if (!Crossings[Index]) { return; }
					CrossBlendPoint(Index);
				},
				NumPoints, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
