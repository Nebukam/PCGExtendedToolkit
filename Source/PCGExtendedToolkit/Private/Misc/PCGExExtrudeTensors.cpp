// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExExtrudeTensors.h"

#include "Data/PCGExData.h"


#define LOCTEXT_NAMESPACE "PCGExExtrudeTensorsElement"
#define PCGEX_NAMESPACE ExtrudeTensors

TArray<FPCGPinProperties> UPCGExExtrudeTensorsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExTensor::SourceTensorsLabel, "Tensors", Required, {})
	PCGEX_PIN_POINT(PCGEx::SourceBoundsLabel, "Bounds in which extrusion will be limited", Normal, {})
	return PinProperties;
}

PCGExData::EIOInit UPCGExExtrudeTensorsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

PCGEX_INITIALIZE_ELEMENT(ExtrudeTensors)

FName UPCGExExtrudeTensorsSettings::GetMainInputPin() const { return PCGExGraph::SourceSeedsLabel; }
FName UPCGExExtrudeTensorsSettings::GetMainOutputPin() const { return PCGExGraph::OutputPathsLabel; }

bool FPCGExExtrudeTensorsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ExtrudeTensors)

	Context->TensorsHandler = MakeShared<PCGExTensor::FTensorsHandler>();
	if (!Context->TensorsHandler->Init(Context, PCGExTensor::SourceTensorsLabel))
	{
		return false;
	}

	if (const TSharedPtr<PCGExData::FPointIO> BoundsData = PCGExData::TryGetSingleInput(Context, PCGEx::SourceBoundsLabel, false))
	{
		// Let's hope bounds are lightweight
		const TArray<FPCGPoint>& InPoints = BoundsData->GetIn()->GetPoints();
		for (const FPCGPoint& Pt : InPoints) { Context->Limits += PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::Bounds>(Pt).TransformBy(Pt.Transform); }
	}

	Context->ClosedLoopSquaredDistance = FMath::Square(Settings->ClosedLoopSearchDistance);
	Context->ClosedLoopSearchDot = PCGExMath::DegreesToDot(Settings->ClosedLoopSearchAngle);

	return true;
}

bool FPCGExExtrudeTensorsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExExtrudeTensorsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ExtrudeTensors)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->AddConsumableAttributeName(Settings->IterationsAttribute);

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExExtrudeTensors::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExExtrudeTensors::FProcessor>>& NewBatch)
			{
				//NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to subdivide."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExExtrudeTensors
{
	FExtrusion::FExtrusion(const int32 InSeedIndex, const TSharedRef<PCGExData::FFacade>& InFacade, const int32 InMaxIterations) :
		ExtrudedPoints(InFacade->GetOut()->GetMutablePoints()), SeedIndex(InSeedIndex), RemainingIterations(InMaxIterations), PointDataFacade(InFacade)
	{
		ExtrudedPoints.Reserve(InMaxIterations);
		Origin = InFacade->Source->GetInPoint(SeedIndex);
		Head = Origin.Transform;
		ExtrudedPoints.Add(Origin);
		Metrics.Add(Head.GetLocation());
	}

	void FExtrusion::SetHead(const FTransform& InHead)
	{
		Head = InHead;
		ExtrudedPoints.Last().Transform = Head;
		Metrics = PCGExPaths::FPathMetrics(Head.GetLocation());
	}

	void FExtrusion::Complete()
	{
		if (bIsComplete || bIsStopped) { return; }

		bIsComplete = true;

		ExtrudedPoints.Shrink();
		if (ExtrudedPoints.Num() <= 1)
		{
			PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::None);
			PointDataFacade->Source->Disable();
			return;
		}

		if (!bIsClosedLoop) { if (Settings->bTagIfOpenPath) { PointDataFacade->Source->Tags->AddRaw(Settings->IsOpenPathTag); } }
		else { if (Settings->bTagIfClosedLoop) { PointDataFacade->Source->Tags->AddRaw(Settings->IsClosedLoopTag); } }

		if (Settings->bTagIfIsStoppedByBounds && bHitBounds) { PointDataFacade->Source->Tags->AddRaw(Settings->IsStoppedByBoundsTag); }
		if (Settings->bTagIfChildExtrusion && bIsChildExtrusion) { PointDataFacade->Source->Tags->AddRaw(Settings->IsChildExtrusionTag); }
		if (Settings->bTagIfIsFollowUp && bIsFollowUp) { PointDataFacade->Source->Tags->AddRaw(Settings->IsFollowUpTag); }

		PointDataFacade->Source->GetOutKeys(true);
	}

	bool FExtrusion::Extrude(const PCGExTensor::FTensorSample& Sample)
	{
		// return whether we can keep extruding or not

		bIsExtruding = true;

		double DistToLast = 0;
		const double Length = Metrics.Add(Metrics.Last + Sample.DirectionAndSize, DistToLast);
		DistToLastSum += DistToLast;

		if (DistToLastSum < Settings->FuseDistance) { return true; }
		DistToLastSum = 0;

		FVector TargetPosition = Metrics.Last;

		if (Length >= MaxLength)
		{
			// Adjust position to match max length
			const FVector LastValidPos = ExtrudedPoints.Last().Transform.GetLocation();
			TargetPosition = LastValidPos + ((TargetPosition - LastValidPos).GetSafeNormal() * (Length - MaxLength));
		}

		Insert();

		return !(Length >= MaxLength || ExtrudedPoints.Num() >= MaxPointCount);
	}

	void FExtrusion::StartNewExtrusion()
	{
		if (RemainingIterations > 1)
		{
			// We re-entered bounds from a previously completed path.
			// TODO : Start a new extrusion from head if selected mode allows for it
			// TODO : Set extrusion origin to be the intersection point with the limit box
			if (const TSharedPtr<FExtrusion> ChildExtrusion = Processor->InitExtrusionFromExtrusion(SharedThis(this)))
			{
				ChildExtrusion->bIsChildExtrusion = true;
				ChildExtrusion->bIsFollowUp = bIsComplete;
			}
		}
	}


	bool FExtrusion::OnAdvanced(const bool bStop)
	{
		RemainingIterations--;

		if (bStop || RemainingIterations <= 0)
		{
			Complete();
			bIsStopped = true;
		}

		return !bIsStopped;
	}


	void FExtrusion::Insert() const
	{
		FPCGPoint& Point = ExtrudedPoints.Emplace_GetRef(ExtrudedPoints.Last());

		Point.Transform = Head;

		if (Settings->bRefreshSeed) { Point.Seed = PCGExRandom::ComputeSeed(Point, FVector(Origin.Seed)); }
	}


	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExExtrudeTensors::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		AttributesToPathTags = Settings->AttributesToPathTags;
		if (!AttributesToPathTags.Init(Context, PointDataFacade)) { return false; }

		if (Settings->bUsePerPointMaxIterations)
		{
			PerPointIterations = PointDataFacade->GetBroadcaster<int32>(Settings->IterationsAttribute, true);
			if (!PerPointIterations)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Iteration attribute is missing on some inputs, they will be ignored."));
				return false;
			}

			if (Settings->bUseMaxFromPoints) { RemainingIterations = FMath::Max(RemainingIterations, PerPointIterations->Max); }
		}
		else
		{
			RemainingIterations = Settings->Iterations;
		}

		if (Settings->bUseMaxLength && Settings->MaxLengthInput == EPCGExInputValueType::Attribute)
		{
			PerPointMaxLength = PointDataFacade->GetBroadcaster<double>(Settings->MaxLengthAttribute);
			if (!PerPointMaxLength)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Max length attribute is missing on some inputs, they will be ignored."));
				return false;
			}
		}

		if (Settings->bUseMaxPointsCount && Settings->MaxPointsCountInput == EPCGExInputValueType::Attribute)
		{
			PerPointMaxPoints = PointDataFacade->GetBroadcaster<int32>(Settings->MaxPointsCountAttribute);
			if (!PerPointMaxPoints)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Max point count attribute is missing on some inputs, they will be ignored."));
				return false;
			}
		}

		const int32 NumPoints = PointDataFacade->GetNum();
		PCGEx::InitArray(ExtrusionQueue, NumPoints);
		PointFilterCache.Init(true, NumPoints);

		Context->MainPoints->IncreaseReserve(NumPoints);

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::InitExtrusionFromSeed(const int32 InSeedIndex)
	{
		if (Settings->bIgnoreOutOfBoundsSeeds && Context->Limits.IsValid)
		{
			const FVector Head = PointDataFacade->Source->GetInPoint(InSeedIndex).Transform.GetLocation();
#if PCGEX_ENGINE_VERSION <= 503
			bool bWithinLimits = Context->Limits.IsInside(Head);
#else
			bool bWithinLimits = Context->Limits.IsInsideOrOn(Head);
#endif
			if (!bWithinLimits) { return; }
		}

		const int32 Iterations = PerPointIterations ? PerPointIterations->Read(InSeedIndex) : Settings->Iterations;
		if (Iterations < 1) { return; }

		const TSharedPtr<FExtrusion> NewExtrusion = CreateExtrusionTemplate(InSeedIndex, Iterations);
		if (!NewExtrusion) { return; }

		if (Settings->bUseMaxLength) { NewExtrusion->MaxLength = PerPointMaxLength ? PerPointMaxLength->Read(InSeedIndex) : Settings->MaxLength; }
		if (Settings->bUseMaxPointsCount) { NewExtrusion->MaxPointCount = PerPointMaxPoints ? PerPointMaxPoints->Read(InSeedIndex) : Settings->MaxPointsCount; }

		ExtrusionQueue[InSeedIndex] = NewExtrusion;
	}

	TSharedPtr<FExtrusion> FProcessor::InitExtrusionFromExtrusion(const TSharedRef<FExtrusion>& InExtrusion)
	{
		if (!Settings->bAllowChildExtrusions) { return nullptr; }

		const TSharedPtr<FExtrusion> NewExtrusion = CreateExtrusionTemplate(InExtrusion->SeedIndex, InExtrusion->RemainingIterations);
		if (!NewExtrusion) { return nullptr; }

		NewExtrusion->SetHead(InExtrusion->Head);

		{
			FWriteScopeLock WriteScopeLock(NewExtrusionLock);
			NewExtrusions.Add(NewExtrusion);
		}

		return NewExtrusion;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope)
	{
		PointDataFacade->Fetch(Scope);
		//FilterScope(Scope);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		//if (!PointFilterCache[Index]) { return; }
		InitExtrusionFromSeed(Index);
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		if (!UpdateExtrusionQueue()) { StartParallelLoopForRange(ExtrusionQueue.Num()); }
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope)
	{
		if (!ExtrusionQueue[Iteration]->Advance())
		{
			ExtrusionQueue[Iteration]->Complete();
			ExtrusionQueue[Iteration] = nullptr;
		}
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		RemainingIterations--;
		// TODO : If detecting collisions is enabled, start detection loop here
		// Note : Closed loop search is probably very redundant here with collision
		// Test only with last edge of each extrusion against all others extrusions including itself
		if (!UpdateExtrusionQueue()) { StartParallelLoopForRange(ExtrusionQueue.Num()); }
	}

	bool FProcessor::UpdateExtrusionQueue()
	{
		if (RemainingIterations <= 0) { return true; }

		int32 WriteIndex = 0;
		for (int i = 0; i < ExtrusionQueue.Num(); i++)
		{
			if (ExtrusionQueue[i]) { ExtrusionQueue[WriteIndex++] = ExtrusionQueue[i]; }
		}

		ExtrusionQueue.SetNum(WriteIndex);

		if (!NewExtrusions.IsEmpty())
		{
			ExtrusionQueue.Reserve(ExtrusionQueue.Num() + NewExtrusions.Num());
			ExtrusionQueue.Append(NewExtrusions);
			NewExtrusions.Reset();
		}

		return ExtrusionQueue.IsEmpty();
	}

	void FProcessor::CompleteWork()
	{
		for (const TSharedPtr<FExtrusion>& E : ExtrusionQueue) { if (E) { E->Complete(); } }
		ExtrusionQueue.Empty();
	}

	TSharedPtr<FExtrusion> FProcessor::CreateExtrusionTemplate(const int32 InSeedIndex, const int32 InMaxIterations)
	{
		const TSharedPtr<PCGExData::FPointIO> NewIO = Context->MainPoints->Emplace_GetRef(PointDataFacade->Source->GetIn(), PCGExData::EIOInit::None);
		if (!NewIO) { return nullptr; }

		PCGEX_MAKE_SHARED(Facade, PCGExData::FFacade, NewIO.ToSharedRef());
		if (!Facade->Source->InitializeOutput(PCGExData::EIOInit::New)) { return nullptr; }

		TSharedPtr<FExtrusion> NewExtrusion = nullptr;

#define PCGEX_NEW_EXTRUSION(_FLAGS) NewExtrusion = MakeShared<TExtrusion<_FLAGS>>(InSeedIndex, Facade.ToSharedRef(), InMaxIterations);
#define PCGEX_1_FLAGS_CASE(_A) case EExtrusionFlags::_A : PCGEX_NEW_EXTRUSION(EExtrusionFlags::_A) break;
#define PCGEX_2_FLAGS(_A, _B) static_cast<EExtrusionFlags>(static_cast<uint32>(EExtrusionFlags::_A) | static_cast<uint32>(EExtrusionFlags::_B))
#define PCGEX_2_FLAGS_CASE(_A, _B) case PCGEX_2_FLAGS(_A, _B) : PCGEX_NEW_EXTRUSION(PCGEX_2_FLAGS(_A, _B)) break;
#define PCGEX_3_FLAGS(_A, _B, _C) static_cast<EExtrusionFlags>(static_cast<uint32>(EExtrusionFlags::_A) | static_cast<uint32>(EExtrusionFlags::_B) | static_cast<uint32>(EExtrusionFlags::_C))
#define PCGEX_3_FLAGS_CASE(_A, _B, _C) case PCGEX_3_FLAGS(_A, _B, _C) : PCGEX_NEW_EXTRUSION(PCGEX_3_FLAGS(_A, _B, _C)) break;
#define PCGEX_4_FLAGS(_A, _B, _C, _D) static_cast<EExtrusionFlags>(static_cast<uint32>(EExtrusionFlags::_A) | static_cast<uint32>(EExtrusionFlags::_B) | static_cast<uint32>(EExtrusionFlags::_C) | static_cast<uint32>(EExtrusionFlags::_D))
#define PCGEX_4_FLAGS_CASE(_A, _B, _C, _D) case PCGEX_4_FLAGS(_A, _B, _C, _D) : PCGEX_NEW_EXTRUSION(PCGEX_4_FLAGS(_A, _B, _C, _D)) break;

		switch (ComputeFlags(Settings, Context))
		{
		PCGEX_1_FLAGS_CASE(None)
		PCGEX_1_FLAGS_CASE(Bounded)
		PCGEX_1_FLAGS_CASE(AllowsChildren)
		PCGEX_1_FLAGS_CASE(ClosedLoop)
		PCGEX_2_FLAGS_CASE(Bounded, AllowsChildren)
		PCGEX_2_FLAGS_CASE(Bounded, ClosedLoop)
		PCGEX_2_FLAGS_CASE(AllowsChildren, ClosedLoop)
		PCGEX_3_FLAGS_CASE(Bounded, AllowsChildren, ClosedLoop)
		default:
			checkNoEntry(); // You missed flags dummy
			break;
		}

#undef PCGEX_NEW_EXTRUSION
#undef PCGEX_1_FLAGS_CASE
#undef PCGEX_2_FLAGS
#undef PCGEX_2_FLAGS_CASE
#undef PCGEX_3_FLAGS
#undef PCGEX_3_FLAGS_CASE
#undef PCGEX_4_FLAGS
#undef PCGEX_4_FLAGS_CASE

		if (Settings->bUseMaxLength) { NewExtrusion->MaxLength = PerPointMaxLength ? PerPointMaxLength->Read(InSeedIndex) : Settings->MaxLength; }
		if (Settings->bUseMaxPointsCount) { NewExtrusion->MaxPointCount = PerPointMaxPoints ? PerPointMaxPoints->Read(InSeedIndex) : Settings->MaxPointsCount; }

		NewExtrusion->PointDataFacade->Source->IOIndex = BatchIndex * 1000000 + InSeedIndex;
		AttributesToPathTags.Tag(InSeedIndex, Facade->Source);

		NewExtrusion->Processor = this;
		NewExtrusion->Context = Context;
		NewExtrusion->Settings = Settings;

		return NewExtrusion;
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
