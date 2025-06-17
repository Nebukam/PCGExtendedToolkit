// Copyright 2025 Timothé Lapetite and contributors
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
	PCGEX_PIN_FACTORIES(PCGExPaths::SourceCanCutFilters, "Fiter which edges can 'cut' other edges. Leave empty so all edges are can cut other edges.", Normal, {})
	PCGEX_PIN_FACTORIES(PCGExPaths::SourceCanBeCutFilters, "Fiter which edges can be 'cut' by other edges. Leave empty so all edges are can cut other edges.", Normal, {})
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExDataBlending::SourceOverridesBlendingOps)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(PathCrossings)

bool FPCGExPathCrossingsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathCrossings)

	if (Settings->IntersectionDetails.bWriteCrossing) { PCGEX_VALIDATE_NAME(Settings->IntersectionDetails.CrossingAttributeName) }
	if (Settings->bWriteAlpha) { PCGEX_VALIDATE_NAME(Settings->CrossingAlphaAttributeName) }
	if (Settings->bWriteCrossDirection) { PCGEX_VALIDATE_NAME(Settings->CrossDirectionAttributeName) }
	if (Settings->bWriteIsPointCrossing) { PCGEX_VALIDATE_NAME(Settings->IsPointCrossingAttributeName) }

	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInstancedFactory, PCGExDataBlending::SourceOverridesBlendingOps)

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

					if (bIsCanBeCutTagValid) { if (Settings->bTagIfHasNoCrossings && Entry->Tags->IsTagged(Context->CanBeCutTag)) { Entry->Tags->AddRaw(Settings->HasNoCrossingsTag); } }
					else if (Settings->bTagIfHasNoCrossings) { Entry->Tags->AddRaw(Settings->HasNoCrossingsTag); }

					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExPathCrossings::FProcessor>>& NewBatch)
			{
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
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathCrossings::Process);

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		// Must be set before process for filters
		//PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		bClosedLoop = PCGExPaths::GetClosedLoop(PointIO->GetIn());
		bSelfIntersectionOnly = Settings->bSelfIntersectionOnly;
		Details = Settings->IntersectionDetails;
		Details.Init();

		CanCutFilterManager = MakeShared<PCGExPointFilter::FManager>(PointDataFacade);
		if (!CanCutFilterManager->Init(ExecutionContext, Context->CanCutFilterFactories)) { CanCutFilterManager.Reset(); }

		CanBeCutFilterManager = MakeShared<PCGExPointFilter::FManager>(PointDataFacade);
		if (!CanBeCutFilterManager->Init(ExecutionContext, Context->CanBeCutFilterFactories)) { CanBeCutFilterManager.Reset(); }

		Path = PCGExPaths::MakePath(PointIO->GetIn(), Details.Tolerance * 2);
		Path->IOIndex = PointDataFacade->Source->IOIndex;
		PathLength = Path->AddExtra<PCGExPaths::FPathEdgeLength>();

		Path->IOIndex = PointIO->IOIndex;

		CanCut.Init(true, Path->NumEdges);
		CanBeCut.Init(true, Path->NumEdges);
		EdgeCrossings.Init(nullptr, Path->NumEdges);

		SubBlending = Context->Blending->CreateOperation();
		SubBlending->bClosedLoop = bClosedLoop;

		if (Settings->bOrientCrossing) { SubBlending->bPreserveRotation = true; }

		bCanBeCut = PCGEx::IsValidStringTag(Context->CanBeCutTag) ? PointDataFacade->Source->Tags->IsTagged(Context->CanBeCutTag, Settings->bInvertCanBeCutTag) : true;
		bCanCut = PCGEx::IsValidStringTag(Context->CanCutTag) ? PointDataFacade->Source->Tags->IsTagged(Context->CanCutTag, Settings->bInvertCanCutTag) : true;

		PCGExMT::FScope EdgesScope = Path->GetEdgeScope();
		if (CanCutFilterManager) { if (CanCutFilterManager->Test(EdgesScope, CanCut)) { bCanCut = false; } }
		if (CanBeCutFilterManager) { if (CanBeCutFilterManager->Test(EdgesScope, CanBeCut) == 0) { bCanBeCut = false; } }
		Path->ComputeAllEdgeExtra();

		CanCutFilterManager.Reset();
		CanBeCutFilterManager.Reset();
		Path->BuildPartialEdgeOctree(CanCut);
		CanCut.Empty();

		return true;
	}

	void FProcessor::CompleteWork()
	{
		if (!bCanBeCut) { return; }
		StartParallelLoopForRange(Path->NumEdges);
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			EdgeCrossings[Index] = nullptr;
			if (!CanBeCut[Index]) { continue; }

			const PCGExPaths::FPathEdge& Edge = Path->Edges[Index];
			if (!Path->IsEdgeValid(Edge)) { continue; }

			const TSharedPtr<PCGExPaths::FPathEdgeCrossings> NewCrossing = MakeShared<PCGExPaths::FPathEdgeCrossings>(Index);

			// Find crossings
			if (bSelfIntersectionOnly)
			{
				Path->GetEdgeOctree()->FindElementsWithBoundsTest(
					Edge.Bounds.GetBox(), [&](const PCGExPaths::FPathEdge* OtherEdge)
					{
						if (Edge.ShareIndices(OtherEdge)) { return; }
						NewCrossing->FindSplit(Path, Edge, PathLength, Path, *OtherEdge, Details);
					});
			}
			else
			{
				for (const TSharedPtr<PCGExPointsMT::IPointsProcessorBatch> Parent = ParentBatch.Pin();
				     const TSharedRef<PCGExData::FFacade>& Facade : Parent->ProcessorFacades)
				{
					const TSharedRef<FPointsProcessor>* OtherProcessorPtr = Parent->SubProcessorMap->Find(&Facade->Source.Get());
					if (!OtherProcessorPtr) { continue; }

					TSharedRef<FPointsProcessor> OtherProcessor = *OtherProcessorPtr;
					if (!Details.bEnableSelfIntersection && &OtherProcessor.Get() == this) { continue; }

					const TSharedRef<FProcessor> TypedProcessor = StaticCastSharedRef<FProcessor>(OtherProcessor);
					if (!TypedProcessor->bCanCut) { continue; }

					TypedProcessor->GetEdgeOctree()->FindElementsWithBoundsTest(
						Edge.Bounds.GetBox(),
						[&](const PCGExPaths::FPathEdge* OtherEdge)
						{
							NewCrossing->FindSplit(Path, Edge, PathLength, TypedProcessor->Path, *OtherEdge, Details);
						});
				}
			}

			if (!NewCrossing->IsEmpty())
			{
				NewCrossing->SortByAlpha();
				EdgeCrossings[Index] = NewCrossing;
			}
		}
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;
		PCGEX_INIT_IO_VOID(PointIO, PCGExData::EIOInit::New)

		int32 NumPointsFinal = 0;

		for (int i = 0; i < Path->NumPoints; i++)
		{
			NumPointsFinal++;

			if (!Path->IsClosedLoop() && i == Path->LastIndex) { continue; }

			const PCGExPaths::FPathEdgeCrossings* Crossing = EdgeCrossings[i].Get();
			if (!Crossing) { continue; }

			NumPointsFinal += Crossing->Crossings.Num();
		}

		const UPCGBasePointData* InPoints = PointIO->GetIn();
		UPCGBasePointData* OutPoints = PointIO->GetOut();
		PCGEx::SetNumPointsAllocated(OutPoints, NumPointsFinal, InPoints->GetAllocatedProperties());

		TArray<int32> WriteIndices;
		WriteIndices.Reserve(InPoints->GetNumPoints());

		UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;


		TConstPCGValueRange<int64> InMetadataEntries = InPoints->GetConstMetadataEntryValueRange();
		TPCGValueRange<int64> OutMetadataEntries = OutPoints->GetMetadataEntryValueRange(false);

		int32 Index = 0;
		for (int i = 0; i < Path->NumEdges; i++)
		{
			Path->Edges[i].AltStart = Index;
			WriteIndices.Add(Index);

			OutMetadataEntries[Index] = InMetadataEntries[i];
			Metadata->InitializeOnSet(OutMetadataEntries[Index++]);

			const PCGExPaths::FPathEdgeCrossings* Crossing = EdgeCrossings[i].Get();
			if (!Crossing) { continue; }

			for (const PCGExPaths::FCrossing X : Crossing->Crossings)
			{
				CrossIOIndices.Add(PCGEx::H64B(X.Hash));
				OutMetadataEntries[Index] = InMetadataEntries[i];
				Metadata->InitializeOnSet(OutMetadataEntries[Index++]);
			}
		}

		if (!Path->IsClosedLoop())
		{
			WriteIndices.Add(Index);
			OutMetadataEntries[Index] = InMetadataEntries[Path->LastIndex];
			Metadata->InitializeOnSet(OutMetadataEntries[Index]);
		}

		// BUG : Missing last (or first?) point
		// We should inherit all points :(
		ensure(WriteIndices.Num() == InPoints->GetNumPoints());

		PointIO->InheritPoints(WriteIndices);

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

		if (Settings->bWriteIsPointCrossing)
		{
			IsPointCrossingWriter = PointDataFacade->GetWritable<bool>(Settings->IsPointCrossingAttributeName, false, true, PCGExData::EBufferInit::New);
			ProtectedAttributes.Add(Settings->IsPointCrossingAttributeName);
		}

		if (!SubBlending->PrepareForData(Context, PointDataFacade, &ProtectedAttributes))
		{
			bIsProcessorValid = false;
			return;
		}

		if (PointIO->GetIn()->GetNumPoints() != PointIO->GetOut()->GetNumPoints()) { if (Settings->bTagIfHasCrossing) { PointIO->Tags->AddRaw(Settings->HasCrossingsTag); } }
		else { if (Settings->bTagIfHasNoCrossings) { PointIO->Tags->AddRaw(Settings->HasNoCrossingsTag); } }

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, CollapseTask)
		CollapseTask->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->PointDataFacade->WriteFastest(This->AsyncManager);
			};

		CollapseTask->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				This->CollapseCrossings(Scope);
			};

		CollapseTask->StartSubLoops(Path->NumEdges, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}

	void FProcessor::CollapseCrossings(const PCGExMT::FScope& Scope)
	{
		TArray<int32> Order;
		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		UPCGBasePointData* OutPoints = PointIO->GetOut();
		TPCGValueRange<FTransform> OutTransforms = OutPoints->GetTransformValueRange(false);

		PCGEX_SCOPE_LOOP(Index)
		{
			PCGExPaths::FPathEdgeCrossings* Crossing = EdgeCrossings[Index].Get();
			const PCGExPaths::FPathEdge& Edge = Path->Edges[Index];

			if (FlagWriter) { FlagWriter->SetValue(Edge.AltStart, false); }
			if (IsPointCrossingWriter) { IsPointCrossingWriter->SetValue(Edge.AltStart, false); }
			if (AlphaWriter) { AlphaWriter->SetValue(Edge.AltStart, Settings->DefaultAlpha); }
			if (CrossWriter) { CrossWriter->SetValue(Edge.AltStart, Settings->DefaultCrossDirection); }

			if (!Crossing) { continue; }

			const int32 NumCrossings = Crossing->Crossings.Num();
			const int32 CrossingStartIndex = Edge.AltStart + 1;

			PCGExPaths::FPathMetrics Metrics = PCGExPaths::FPathMetrics(Path->GetPos(Edge.Start));


			for (int i = 0; i < NumCrossings; i++)
			{
				const PCGExPaths::FCrossing& Itx = Crossing->Crossings[i];
				const int32 PointIndex = CrossingStartIndex + i;

				if (FlagWriter) { FlagWriter->SetValue(PointIndex, true); }
				if (AlphaWriter) { AlphaWriter->SetValue(PointIndex, Itx.Alpha); }
				if (CrossWriter) { CrossWriter->SetValue(PointIndex, Itx.Dir); }
				if (IsPointCrossingWriter) { IsPointCrossingWriter->SetValue(PointIndex, Itx.bIsPoint); }

				if (Settings->bOrientCrossing) { OutTransforms[PointIndex].SetRotation(PCGExMath::MakeDirection(Settings->CrossingOrientAxis, Itx.Dir)); }
				OutTransforms[PointIndex].SetLocation(Itx.Location);

				Metrics.Add(Itx.Location);
			}

			Metrics.Add(Path->GetPos(Edge.End));

			const int32 EndIndex = Index == Path->LastIndex ? 0 : CrossingStartIndex + NumCrossings;
			PCGExData::FScope SubScope = PointIO->GetOutScope(CrossingStartIndex, NumCrossings);
			SubBlending->ProcessSubPoints(PointIO->GetOutPoint(CrossingStartIndex - 1), PointIO->GetOutPoint(EndIndex), SubScope, Metrics);
		}
	}

	void FProcessor::CrossBlend(const PCGExMT::FScope& Scope)
	{
		TArray<int32> Order;
		TArray<PCGExData::FWeightedPoint> WeightedPoints;
		TArray<PCGEx::FOpStats> Trackers;

		UnionBlender->InitTrackers(Trackers);

		const TSharedPtr<PCGExData::IUnionData> TempUnion = MakeShared<PCGExData::IUnionData>();

		PCGEX_SCOPE_LOOP(Index)
		{
			const PCGExPaths::FPathEdgeCrossings* Crossing = EdgeCrossings[Index].Get();
			if (!Crossing) { continue; }

			const PCGExPaths::FPathEdge& Edge = Path->Edges[Index];
			const int32 NumCrossings = Crossing->Crossings.Num();

			for (int i = 0; i < NumCrossings; i++)
			{
				const PCGExPaths::FCrossing& Itx = Crossing->Crossings[i];

				uint32 PtIdx;
				uint32 IOIdx;
				PCGEx::H64(Itx.Hash, PtIdx, IOIdx);

				const int32 SecondIndex = PtIdx + 1 >= static_cast<uint32>(Context->MainPoints->Pairs[IOIdx]->GetNum(PCGExData::EIOSide::In)) ? 0 : PtIdx + 1;

				TempUnion->Reset();
				TempUnion->Add(PCGExData::FPoint(PtIdx, IOIdx));
				TempUnion->Add(PCGExData::FPoint(SecondIndex, IOIdx));

				UnionBlender->MergeSingle(Edge.AltStart + i + 1, TempUnion, WeightedPoints, Trackers);
			}
		}
	}

	void FProcessor::Write()
	{
		if (!bCanBeCut)
		{
			PCGEX_INIT_IO_VOID(PointDataFacade->Source, PCGExData::EIOInit::Forward)
			return;
		}

		const TSharedPtr<PCGExDataBlending::FUnionBlender> TypedBlender = MakeShared<PCGExDataBlending::FUnionBlender>(&Settings->CrossingBlending, &Settings->CrossingCarryOver, Context->Distances);
		UnionBlender = TypedBlender;

		TArray<TSharedRef<PCGExData::FFacade>> UnionSources;
		UnionSources.Reserve(Context->MainPoints->Pairs.Num());

		for (const TSharedPtr<PCGExData::FPointIO>& IO : Context->MainPoints->Pairs)
		{
			if (IO && CrossIOIndices.Contains(IO->IOIndex)) { UnionSources.Add(Context->SubProcessorMap[IO.Get()]->PointDataFacade); }
		}

		TypedBlender->AddSources(UnionSources, &ProtectedAttributes);

		if (!TypedBlender->Init(Context, PointDataFacade, true))
		{
			// TODO : Log error
			bIsProcessorValid = false;
			return;
		}

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, CrossBlendTask)
		CrossBlendTask->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				This->CrossBlend(Scope);
			};

		CrossBlendTask->StartSubLoops(Path->NumEdges, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
