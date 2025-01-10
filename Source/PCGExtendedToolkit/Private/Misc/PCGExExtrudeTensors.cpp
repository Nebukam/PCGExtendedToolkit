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
		Head = Origin.Transform.GetLocation();
		ExtrudedPoints.Add(Origin);
		Metrics.Add(Head);
	}

	void FExtrusion::SetOriginPosition(const FVector& InPosition)
	{
		Head = InPosition;
		ExtrudedPoints.Last().Transform.SetLocation(Head);
		Metrics = PCGExPaths::FPathMetrics(Head);
	}

	bool FExtrusion::Advance()
	{
		if (bIsStopped) { return false; }

		auto Exit = [&](const bool bStop)-> bool
		{
			RemainingIterations--;

			if (bStop || RemainingIterations <= 0)
			{
				Complete();
				bIsStopped = true;
			}

			return !bIsStopped;
		};

		const FVector PreviousHead = Head;
		bool bSuccess = false;
		const PCGExTensor::FTensorSample Sample = Context->TensorsHandler->SampleAtPosition(Head, bSuccess);

		if (!bSuccess) { return Exit(true); }

		Head += Sample.DirectionAndSize;

		if (Settings->bSearchForClosedLoops)
		{
			const FVector Tail = Origin.Transform.GetLocation();
			if (FVector::DistSquared(Metrics.Last, Tail) <= Context->ClosedLoopSquaredDistance &&
				FVector::DotProduct(Sample.DirectionAndSize.GetSafeNormal(), (Tail - PreviousHead).GetSafeNormal()) > Context->ClosedLoopSearchDot)
			{
				bIsClosedLoop = true;
				return Exit(true);
			}
		}

		if (Context->Limits.IsValid)
		{
			// Make sure we are within bounds
#if PCGEX_ENGINE_VERSION <= 503
			bool bWithinLimits = Context->Limits.IsInside(Head);
#else
			bool bWithinLimits = Context->Limits.IsInsideOrOn(Head);
#endif

			if (!bWithinLimits)
			{
				if (bIsExtruding && !bIsComplete)
				{
					if (Settings->OutOfBoundHandling == EPCGExOutOfBoundPathPointHandling::Exclude)
					{
						// Nothing to do
					}
					else if (Settings->OutOfBoundHandling == EPCGExOutOfBoundPathPointHandling::Include) { Insert(Sample, Head); }
					else if (Settings->OutOfBoundHandling == EPCGExOutOfBoundPathPointHandling::IncludeAndSnap) { Insert(Sample, Head); }
					Complete();
				}

				return Exit(false);
			}

			if (bIsComplete)
			{
				if (RemainingIterations > 1)
				{
					// We re-entered bounds from a previously completed path.
					// TODO : Start a new extrusion from head if selected mode allows for it
					// TODO : Set extrusion origin to be the intersection point with the limit box
					Processor->InitExtrusionFromExtrusion(SharedThis(this));
				}
				return Exit(true);
			}

			if (!bIsExtruding)
			{
				// Start writing path
				Metrics = PCGExPaths::FPathMetrics(PreviousHead);
			}
		}

		return Exit(!Extrude(Sample));
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

		Insert(Sample, TargetPosition);

		return !(Length >= MaxLength || ExtrudedPoints.Num() >= MaxPointCount);
	}

	void FExtrusion::Insert(const PCGExTensor::FTensorSample& Sample, const FVector& InPosition) const
	{
		FPCGPoint& Point = ExtrudedPoints.Emplace_GetRef(ExtrudedPoints.Last());

		//const FTransform Composite = Point.Transform * Sample.Transform;

		if (Settings->bTransformRotation)
		{
			if (Settings->Rotation == EPCGExTensorTransformMode::Absolute)
			{
				Point.Transform.SetRotation(Sample.Rotation);
			}
			else if (Settings->Rotation == EPCGExTensorTransformMode::Relative)
			{
				Point.Transform.SetRotation(Point.Transform.GetRotation() * Sample.Rotation);
			}
			else if (Settings->Rotation == EPCGExTensorTransformMode::Align)
			{
				Point.Transform.SetRotation(PCGExMath::MakeDirection(Settings->AlignAxis, Sample.DirectionAndSize.GetSafeNormal() * -1, Point.Transform.GetRotation().GetUpVector()));
			}
		}

		Point.Transform.SetLocation(InPosition);
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

		if (!bIsClosedLoop) { if (Settings->bTagIfOpenPath) { PointDataFacade->Source->Tags->Add(Settings->IsOpenPathTag); } }
		else { if (Settings->bTagIfClosedLoop) { PointDataFacade->Source->Tags->Add(Settings->IsClosedLoopTag); } }

		PointDataFacade->Source->GetOutKeys(true);
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

	void FProcessor::PrepareExtrusion(const TSharedPtr<FExtrusion>& InExtrusion)
	{
		InExtrusion->PointDataFacade->Source->IOIndex = BatchIndex * 1000000 + InExtrusion->SeedIndex;
		AttributesToPathTags.Tag(InExtrusion->SeedIndex, InExtrusion->PointDataFacade->Source);

		InExtrusion->Processor = this;
		InExtrusion->Context = Context;
		InExtrusion->Settings = Settings;
	}

	void FProcessor::InitExtrusionFromSeed(const int32 InSeedIndex)
	{
		const int32 Iterations = PerPointIterations ? PerPointIterations->Read(InSeedIndex) : Settings->Iterations;
		if (Iterations < 1) { return; }

		const TSharedPtr<PCGExData::FPointIO> NewIO = Context->MainPoints->Emplace_GetRef(PointDataFacade->Source->GetIn(), PCGExData::EIOInit::None);
		if (!NewIO) { return; }

		PCGEX_MAKE_SHARED(Facade, PCGExData::FFacade, NewIO.ToSharedRef());
		PCGEX_INIT_IO_VOID(Facade->Source, PCGExData::EIOInit::New);
		PCGEX_MAKE_SHARED(NewExtrusion, FExtrusion, InSeedIndex, Facade.ToSharedRef(), Iterations);

		PrepareExtrusion(NewExtrusion);

		if (Settings->bUseMaxLength) { NewExtrusion->MaxLength = PerPointMaxLength ? PerPointMaxLength->Read(InSeedIndex) : Settings->MaxLength; }
		if (Settings->bUseMaxPointsCount) { NewExtrusion->MaxPointCount = PerPointMaxPoints ? PerPointMaxPoints->Read(InSeedIndex) : Settings->MaxPointsCount; }

		ExtrusionQueue[InSeedIndex] = NewExtrusion;
	}

	void FProcessor::InitExtrusionFromExtrusion(const TSharedRef<FExtrusion>& InExtrusion)
	{
		if (!Settings->bAllowNewExtrusions) { return; }

		const TSharedPtr<PCGExData::FPointIO> NewIO = Context->MainPoints->Emplace_GetRef(PointDataFacade->Source->GetIn(), PCGExData::EIOInit::None);
		if (!NewIO) { return; }

		PCGEX_MAKE_SHARED(Facade, PCGExData::FFacade, NewIO.ToSharedRef());
		PCGEX_INIT_IO_VOID(Facade->Source, PCGExData::EIOInit::New);
		PCGEX_MAKE_SHARED(NewExtrusion, FExtrusion, InExtrusion->SeedIndex, Facade.ToSharedRef(), InExtrusion->RemainingIterations);

		PrepareExtrusion(NewExtrusion);
		NewExtrusion->SetOriginPosition(InExtrusion->Head);

		if (Settings->bUseMaxLength) { NewExtrusion->MaxLength = PerPointMaxLength ? PerPointMaxLength->Read(InExtrusion->SeedIndex) : Settings->MaxLength; }
		if (Settings->bUseMaxPointsCount) { NewExtrusion->MaxPointCount = PerPointMaxPoints ? PerPointMaxPoints->Read(InExtrusion->SeedIndex) : Settings->MaxPointsCount; }

		{
			FWriteScopeLock WriteScopeLock(NewExtrusionLock);
			NewExtrusions.Add(NewExtrusion);
		}
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
		};
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		AsyncManager->FlushTasks(); // TODO Check if this is safe, we need to flush iteration tasks before creating new ones
		
		RemainingIterations--;
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
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
