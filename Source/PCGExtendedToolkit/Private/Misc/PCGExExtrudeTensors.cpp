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
	FExtrusion::FExtrusion(const int32 InIndex, const TSharedRef<PCGExData::FFacade>& InFacade, const int32 InMaxIterations)
		: ExtrudedPoints(InFacade->GetOut()->GetMutablePoints()), Index(InIndex), RemainingIterations(InMaxIterations), PointDataFacade(InFacade)
	{
		ExtrudedPoints.Reserve(InMaxIterations);
		Origin = InFacade->Source->GetInPoint(Index);
		ExtrudedPoints.Add(Origin);
		Metrics.Add(Origin.Transform.GetLocation());
	}

	bool FExtrusion::Extrude()
	{
		auto Exit = [&](const bool bStop)-> bool
		{
			RemainingIterations--;

			if (bStop || RemainingIterations <= 0)
			{
				Complete();
				return false;
			}

			return true;
		};

		bool bSuccess = false;
		const PCGExTensor::FTensorSample Sample = TensorsHandler->SampleAtPosition(Metrics.Last, bSuccess);
		if (!bSuccess) { return Exit(true); }

		double DistToLast = 0;
		const double Length = Metrics.Add(Metrics.Last + Sample.DirectionAndSize, DistToLast);
		DistToLastSum += DistToLast;

		if (DistToLastSum < Settings->FuseDistance) { return Exit(false); }
		DistToLastSum = 0;

		FVector TargetPosition = Metrics.Last;

		if (Length >= MaxLength)
		{
			// Adjust position to match max length
			const FVector LastValidPos = ExtrudedPoints.Last().Transform.GetLocation();
			TargetPosition = LastValidPos + ((TargetPosition - LastValidPos).GetSafeNormal() * (Length - MaxLength));
		}

		FPCGPoint& Point = ExtrudedPoints.Add_GetRef(Origin);

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

		Point.Transform.SetLocation(TargetPosition);

		return Exit(Length >= MaxLength || ExtrudedPoints.Num() >= MaxPointCount);
	}

	bool FExtrusion::Complete()
	{
		ExtrudedPoints.Shrink();
		if (ExtrudedPoints.Num() <= 1)
		{
			PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::None);
			PointDataFacade->Source->Disable();
			return false;
		}

		PointDataFacade->Source->GetOutKeys(true);
		return true;
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

		if (Settings->IterationsInput == EPCGExInputValueType::Attribute)
		{
			PerPointIterations = PointDataFacade->GetBroadcaster<int32>(Settings->IterationsAttribute, true);
			if (!PerPointIterations)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Iteration attribute is missing on some inputs, they will be ignored."));
				return false;
			}
			RemainingIterations = PerPointIterations->Max;
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
		PCGEx::InitArray(Extrusions, NumPoints);
		PointFilterCache.Init(true, NumPoints);

		Context->MainPoints->IncreaseReserve(NumPoints);

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, InitExtrusionData)
		InitExtrusionData->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->StartParallelLoopForPoints(PCGExData::ESource::In);
			};


		InitExtrusionData->OnIterationCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const int32 Index, const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				This->InitExtrusion(Index);
			};

		InitExtrusionData->StartIterations(NumPoints, 128);

		return true;
	}

	void FProcessor::InitExtrusion(const int32 Index)
	{
		const int32 Iterations = PerPointIterations ? PerPointIterations->Read(Index) : Settings->Iterations;
		if (Iterations < 1) { return; }

		const TSharedPtr<PCGExData::FPointIO> NewIO = Context->MainPoints->Emplace_GetRef(PointDataFacade->Source->GetIn(), PCGExData::EIOInit::None);
		if (!NewIO) { return; }


		NewIO->IOIndex = BatchIndex * 1000000 + Index;
		AttributesToPathTags.Tag(Index, NewIO);

		PCGEX_MAKE_SHARED(Facade, PCGExData::FFacade, NewIO.ToSharedRef());
		PCGEX_INIT_IO_VOID(Facade->Source, PCGExData::EIOInit::New);

		PCGEX_MAKE_SHARED(NewExtrusion, FExtrusion, Index, Facade.ToSharedRef(), Iterations);
		NewExtrusion->TensorsHandler = Context->TensorsHandler;
		if (Settings->bUseMaxLength) { NewExtrusion->MaxLength = PerPointMaxLength ? PerPointMaxLength->Read(Index) : Settings->MaxLength; }
		if (Settings->bUseMaxPointsCount) { NewExtrusion->MaxPointCount = PerPointMaxPoints ? PerPointMaxPoints->Read(Index) : Settings->MaxPointsCount; }
		NewExtrusion->Settings = Settings;

		Extrusions[Index] = NewExtrusion;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope)
	{
		if (bIteratedOnce) { return; }
		PointDataFacade->Fetch(Scope);
		//FilterScope(Scope);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		if (!PointFilterCache[Index]) { return; }

		TSharedPtr<FExtrusion> Extrusion = Extrusions[Index];
		if (!Extrusion)
		{
			PointFilterCache[Index] = false;
			return;
		}

		PointFilterCache[Index] = Extrusions[Index]->Extrude();
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		bIteratedOnce = true;
		RemainingIterations--;
		if (RemainingIterations > 0)
		{
			// Exit early if we can't extrude anything
			for (const int8 Valid : PointFilterCache)
			{
				if (Valid)
				{
					StartParallelLoopForPoints(PCGExData::ESource::In);
					break;
				}
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
