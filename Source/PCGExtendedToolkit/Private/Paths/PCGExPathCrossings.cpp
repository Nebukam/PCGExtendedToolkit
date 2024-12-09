// Copyright 2024 Timothé Lapetite and contributors
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
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExDataBlending::SourceOverridesBlendingOps)
	return PinProperties;
}

PCGExData::EIOInit UPCGExPathCrossingsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

PCGEX_INITIALIZE_ELEMENT(PathCrossings)

bool FPCGExPathCrossingsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathCrossings)

	if (Settings->IntersectionDetails.bWriteCrossing) { PCGEX_VALIDATE_NAME(Settings->IntersectionDetails.CrossingAttributeName) }
	if (Settings->bWriteAlpha) { PCGEX_VALIDATE_NAME(Settings->CrossingAlphaAttributeName) }
	if (Settings->bWriteCrossDirection) { PCGEX_VALIDATE_NAME(Settings->CrossDirectionAttributeName) }

	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendOperation, PCGExDataBlending::SourceOverridesBlendingOps)

	GetInputFactories(Context, PCGExPaths::SourceCanCutFilters, Context->CanCutFilterFactories, PCGExFactories::PointFilters, false);
	GetInputFactories(Context, PCGExPaths::SourceCanBeCutFilters, Context->CanBeCutFilterFactories, PCGExFactories::PointFilters, false);

	Context->Distances = PCGExDetails::MakeDistances();
	Context->CrossingBlending = Settings->CrossingBlending;

	Context->CanCutTag = PCGEx::StringTagFromName(Settings->CanCutTag);
	Context->CanBeCutTag = PCGEx::StringTagFromName(Settings->CanBeCutTag);

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

		const bool bIsCanBeCutTagValid = PCGEx::IsValidStringTag(Context->CanBeCutTag);

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPathCrossings::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					Entry->InitializeOutput(PCGExData::EIOInit::Forward); // TODO : This is no good as we'll be missing template attributes
					bHasInvalidInputs = true;

					if (bIsCanBeCutTagValid) { if (Settings->bTagIfHasNoCrossings && Entry->Tags->IsTagged(Context->CanBeCutTag)) { Entry->Tags->Add(Settings->HasNoCrossingsTag); } }
					else if (Settings->bTagIfHasNoCrossings) { Entry->Tags->Add(Settings->HasNoCrossingsTag); }

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

		Path = PCGExPaths::MakePath(PointIO->GetIn()->GetPoints(), Details.Tolerance * 2, bClosedLoop);
		PathLength = Path->AddExtra<PCGExPaths::FPathEdgeLength>();

		Path->IOIndex = PointIO->IOIndex;

		Crossings.Init(nullptr, Path->NumEdges);

		CanCut.SetNumUninitialized(Path->NumEdges);
		CanBeCut.SetNumUninitialized(Path->NumEdges);

		Blending = Cast<UPCGExSubPointsBlendOperation>(PrimaryOperation);
		Blending->bClosedLoop = bClosedLoop;
		if (Settings->bOrientCrossing) { Blending->bPreserveRotation = true; }

		//TWeakPtr<FProcessor> WeakPtr = SharedThis(this);

		bCanBeCut = PCGEx::IsValidStringTag(Context->CanBeCutTag) ? PointDataFacade->Source->Tags->IsTagged(Context->CanBeCutTag) : true;
		bCanCut = PCGEx::IsValidStringTag(Context->CanCutTag) ? PointDataFacade->Source->Tags->IsTagged(Context->CanCutTag) : true;

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, Preparation)

		Preparation->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS

				This->CanCutFilterManager.Reset();
				This->CanBeCutFilterManager.Reset();
				This->Path->BuildPartialEdgeOctree(This->CanCut);
				This->CanCut.Empty();
			};

		Preparation->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS

				This->PointDataFacade->Fetch(Scope);

				if (This->CanCutFilterManager && This->CanBeCutFilterManager)
				{
					for (int i = Scope.Start; i < Scope.End; i++)
					{
						This->CanCut[i] = This->CanCutFilterManager->Test(i);
						This->CanBeCut[i] = This->CanBeCutFilterManager->Test(i);
						This->Path->ComputeEdgeExtra(i);
					}
				}
				else if (This->CanCutFilterManager)
				{
					for (int i = Scope.Start; i < Scope.End; i++)
					{
						This->CanCut[i] = This->CanCutFilterManager->Test(i);
						This->CanBeCut[i] = true;
						This->Path->ComputeEdgeExtra(i);
					}
				}
				else if (This->CanBeCutFilterManager)
				{
					for (int i = Scope.Start; i < Scope.End; i++)
					{
						This->CanCut[i] = true;
						This->CanBeCut[i] = This->CanBeCutFilterManager->Test(i);
						This->Path->ComputeEdgeExtra(i);
					}
				}
				else
				{
					for (int i = Scope.Start; i < Scope.End; i++)
					{
						This->CanCut[i] = true;
						This->CanBeCut[i] = true;
						This->Path->ComputeEdgeExtra(i);
					}
				}
			};

		Preparation->StartSubLoops(Path->NumEdges, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());

		return true;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope)
	{
		Crossings[Iteration] = nullptr;
		if (!CanBeCut[Iteration]) { return; }

		const PCGExPaths::FPathEdge& Edge = Path->Edges[Iteration];

		const PCGExPaths::FPath* OtherPath = nullptr;

		int32 CurrentIOIndex = PointDataFacade->Source->IOIndex;

		const TSharedPtr<FCrossing> NewCrossing = MakeShared<FCrossing>(Iteration);

		auto FindSplit = [&](const PCGExPaths::FPathEdge& E1, const PCGExPaths::FPathEdge& E2)
		{
			if (!Path->IsEdgeValid(E1) || !OtherPath->IsEdgeValid(E2)) { return; }

			const FVector& A1 = Path->GetPos(E1.Start);
			const FVector& B1 = Path->GetPos(E1.End);
			const FVector& A2 = OtherPath->GetPos(E2.Start);
			const FVector& B2 = OtherPath->GetPos(E2.End);
			if (A1 == A2 || A1 == B2 || A2 == B1 || B2 == B1) { return; }

			const FVector CrossDir = (B2 - A2).GetSafeNormal();

			if (Details.bUseMinAngle || Details.bUseMaxAngle)
			{
				if (!Details.CheckDot(FMath::Abs(FVector::DotProduct((B1 - A1).GetSafeNormal(), CrossDir)))) { return; }
			}

			FVector A;
			FVector B;
			FMath::SegmentDistToSegment(A1, B1, A2, B2, A, B);
			if (A == A1 || A == B1 || B == A2 || B == B2) { return; }

			if (FVector::DistSquared(A, B) >= Details.ToleranceSquared) { return; }

			NewCrossing->Crossings.Add(PCGEx::H64(E2.Start, CurrentIOIndex));
			NewCrossing->Positions.Add(FMath::Lerp(A, B, 0.5));
			NewCrossing->CrossingDirections.Add(CrossDir);
			NewCrossing->Alphas.Add(FVector::Dist(A1, A) / PathLength->Get(E1));
		};

		// Find crossings
		if (bSelfIntersectionOnly)
		{
			OtherPath = Path.Get();
			Path->GetEdgeOctree()->FindElementsWithBoundsTest(
				Edge.Bounds.GetBox(), [&](const PCGExPaths::FPathEdge* OtherEdge)
				{
					if (Edge.ShareIndices(OtherEdge)) { return; }
					FindSplit(Edge, *OtherEdge);
				});
			return;
		}

		for (const TSharedPtr<PCGExPointsMT::FPointsProcessorBatchBase> Parent = ParentBatch.Pin();
		     const TSharedRef<PCGExData::FFacade>& Facade : Parent->ProcessorFacades)
		{
			const TSharedRef<FPointsProcessor>* OtherProcessorPtr = Parent->SubProcessorMap->Find(&Facade->Source.Get());
			if (!OtherProcessorPtr) { continue; }

			TSharedRef<FPointsProcessor> OtherProcessor = *OtherProcessorPtr;
			if (!Details.bEnableSelfIntersection && &OtherProcessor.Get() == this) { continue; }

			const TSharedRef<FProcessor> TypedProcessor = StaticCastSharedRef<FProcessor>(OtherProcessor);
			if (!TypedProcessor->bCanCut) { return; }

			CurrentIOIndex = TypedProcessor->PointDataFacade->Source->IOIndex;

			OtherPath = TypedProcessor->Path.Get();
			TypedProcessor->GetEdgeOctree()->FindElementsWithBoundsTest(Edge.Bounds.GetBox(), [&](const PCGExPaths::FPathEdge* OtherEdge) { FindSplit(Edge, *OtherEdge); });
		}

		if (!NewCrossing->Crossings.IsEmpty()) { Crossings[Iteration] = NewCrossing; }
	}


	void FProcessor::OnRangeProcessingComplete()
	{
		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		int32 NumPointsFinal = 0;

		for (int i = 0; i < Path->NumPoints; i++)
		{
			NumPointsFinal++;

			if (!Path->IsClosedLoop() && i == Path->LastIndex) { continue; }

			const FCrossing* Crossing = Crossings[i].Get();
			if (!Crossing) { continue; }

			NumPointsFinal += Crossing->Crossings.Num();
		}

		PointIO->InitializeOutput(PCGExData::EIOInit::New);

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		TArray<FPCGPoint>& OutPoints = PointIO->GetOut()->GetMutablePoints();
		UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;

		PCGEx::InitArray(OutPoints, NumPointsFinal);

		int32 Index = 0;
		for (int i = 0; i < Path->NumEdges; i++)
		{
			Path->Edges[i].AltStart = Index;

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

		if (!Path->IsClosedLoop())
		{
			const FPCGPoint& OriginalPoint = InPoints[Path->LastIndex];
			OutPoints[Index] = OriginalPoint;
			Metadata->InitializeOnSet(OutPoints[Index].MetadataEntry);
		}

		// Flag last so it doesn't get captured by blenders
		if (Settings->IntersectionDetails.bWriteCrossing)
		{
			FlagWriter = PointDataFacade->GetWritable(Settings->IntersectionDetails.CrossingAttributeName, false, true, PCGExData::EBufferInit::New);
			ProtectedAttributes.Add(Settings->IntersectionDetails.CrossingAttributeName);
		}

		if (Settings->bWriteAlpha)
		{
			AlphaWriter = PointDataFacade->GetWritable<double>(Settings->CrossingAlphaAttributeName, Settings->DefaultAlpha, true, PCGExData::EBufferInit::New);
			ProtectedAttributes.Add(Settings->CrossingAlphaAttributeName);
		}

		if (Settings->bWriteCrossDirection)
		{
			CrossWriter = PointDataFacade->GetWritable<FVector>(Settings->CrossDirectionAttributeName, Settings->DefaultCrossDirection, true, PCGExData::EBufferInit::New);
			ProtectedAttributes.Add(Settings->CrossDirectionAttributeName);
		}

		Blending->PrepareForData(PointDataFacade, PointDataFacade, PCGExData::ESource::Out, &ProtectedAttributes);

		if (PointIO->GetIn()->GetPoints().Num() != PointIO->GetOut()->GetPoints().Num()) { if (Settings->bTagIfHasCrossing) { PointIO->Tags->Add(Settings->HasCrossingsTag); } }
		else { if (Settings->bTagIfHasNoCrossings) { PointIO->Tags->Add(Settings->HasNoCrossingsTag); } }

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, CollapseTask)
		CollapseTask->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->PointDataFacade->Write(This->AsyncManager);
			};

		CollapseTask->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				for (int i = Scope.Start; i < Scope.End; i++) { This->CollapseCrossing(i); }
			};
		CollapseTask->StartSubLoops(Path->NumEdges, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}

	void FProcessor::CollapseCrossing(const int32 Index)
	{
		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		const FCrossing* Crossing = Crossings[Index].Get();
		const PCGExPaths::FPathEdge& Edge = Path->Edges[Index];

		if (FlagWriter) { FlagWriter->GetMutable(Edge.AltStart) = false; }
		if (AlphaWriter) { AlphaWriter->GetMutable(Edge.AltStart) = Settings->DefaultAlpha; }
		if (CrossWriter) { CrossWriter->GetMutable(Edge.AltStart) = Settings->DefaultCrossDirection; }

		if (!Crossing) { return; }

		const int32 NumCrossings = Crossing->Crossings.Num();
		const int32 CrossingStartIndex = Edge.AltStart + 1;

		TArray<FPCGPoint>& OutPoints = PointIO->GetOut()->GetMutablePoints();
		PCGExPaths::FPathMetrics Metrics = PCGExPaths::FPathMetrics(Path->GetPos(Edge.Start));

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

		Metrics.Add(Path->GetPos(Edge.End));

		const TArrayView<FPCGPoint> View = MakeArrayView(OutPoints.GetData() + CrossingStartIndex, NumCrossings);
		const int32 EndIndex = Index == Path->LastIndex ? 0 : CrossingStartIndex + NumCrossings;
		Blending->ProcessSubPoints(PointIO->GetOutPointRef(CrossingStartIndex - 1), PointIO->GetOutPointRef(EndIndex), View, Metrics, CrossingStartIndex);
	}

	void FProcessor::CrossBlendPoint(const int32 Index)
	{
		const FCrossing* Crossing = Crossings[Index].Get();
		const PCGExPaths::FPathEdge& Edge = Path->Edges[Index];

		const int32 NumCrossings = Crossing->Crossings.Num();

		// Sorting again, ugh
		TArray<int32> Order;
		PCGEx::ArrayOfIndices(Order, NumCrossings);
		Order.Sort([&](const int32 A, const int32 B) { return Crossing->Alphas[A] < Crossing->Alphas[B]; });

		const TSharedPtr<PCGExData::FUnionData> Union = MakeShared<PCGExData::FUnionData>();
		for (int i = 0; i < NumCrossings; i++)
		{
			uint32 PtIdx;
			uint32 IOIdx;
			PCGEx::H64(Crossing->Crossings[Order[i]], PtIdx, IOIdx);

			const int32 SecondIndex = PtIdx + 1 >= static_cast<uint32>(Context->MainPoints->Pairs[IOIdx]->GetNum(PCGExData::ESource::In)) ? 0 : PtIdx + 1;

			Union->Reset();
			Union->Add(IOIdx, PtIdx);
			Union->Add(IOIdx, SecondIndex);
			UnionBlender->SoftMergeSingle(Edge.AltStart + i + 1, Union, Context->Distances);
		}
	}

	void FProcessor::CompleteWork()
	{
		if (!bCanBeCut) { return; }
		StartParallelLoopForRange(Path->NumEdges);
	}

	void FProcessor::Write()
	{
		if (!bCanBeCut)
		{
			PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::Forward);
			return;
		}

		UnionMetadata = MakeShared<PCGExData::FUnionMetadata>();
		UnionBlender = MakeShared<PCGExDataBlending::FUnionBlender>(&Settings->CrossingBlending, &Settings->CrossingCarryOver);
		for (const TSharedPtr<PCGExData::FPointIO>& IO : Context->MainPoints->Pairs)
		{
			if (IO && CrossIOIndices.Contains(IO->IOIndex)) { UnionBlender->AddSource(Context->SubProcessorMap[IO.Get()]->PointDataFacade, &ProtectedAttributes); }
		}

		UnionBlender->PrepareSoftMerge(Context, PointDataFacade, UnionMetadata);

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, CrossBlendTask)
		CrossBlendTask->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				for (int i = Scope.Start; i < Scope.End; i++)
				{
					if (!This->Crossings[i]) { continue; }
					This->CrossBlendPoint(i);
				}
			};
		CrossBlendTask->StartSubLoops(Path->NumEdges, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
