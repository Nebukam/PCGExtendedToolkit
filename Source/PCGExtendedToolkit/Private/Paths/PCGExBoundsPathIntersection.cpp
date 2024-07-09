// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExBoundsPathIntersection.h"

#define LOCTEXT_NAMESPACE "PCGExBoundsPathIntersectionElement"
#define PCGEX_NAMESPACE BoundsPathIntersection

TArray<FPCGPinProperties> UPCGExBoundsPathIntersectionSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceBoundsLabel, "Intersection points (bounds)", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExBoundsPathIntersectionSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(BoundsPathIntersection)

FPCGExBoundsPathIntersectionContext::~FPCGExBoundsPathIntersectionContext()
{
	PCGEX_TERMINATE_ASYNC

	if (BoundsDataFacade) { PCGEX_DELETE(BoundsDataFacade->Source) }
	PCGEX_DELETE(BoundsDataFacade)
}

bool FPCGExBoundsPathIntersectionElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BoundsPathIntersection)

	PCGExData::FPointIO* BoundsIO = PCGExData::TryGetSingleInput(InContext, PCGEx::SourceBoundsLabel, true);
	if (!BoundsIO) { return false; }

	Context->BoundsDataFacade = new PCGExData::FFacade(BoundsIO);

	return true;
}

bool FPCGExBoundsPathIntersectionElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBoundsPathIntersectionElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BoundsPathIntersection)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bHasInvalildInputs = false;
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPathIntersections::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 2)
				{
					if (!Settings->bOmitSinglePointOutputs) { Entry->InitializeOutput(PCGExData::EInit::Forward); }
					else { bHasInvalildInputs = true; }
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExPathIntersections::FProcessor>* NewBatch)
			{
				NewBatch->SetPointsFilterData(&Context->FilterFactories);
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any paths to intersect with."));
			return true;
		}

		if (bHasInvalildInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->OutputMainPoints();
	Context->Done();

	return Context->TryComplete();
}

namespace PCGExPathIntersections
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(Segmentation)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BoundsPathIntersection)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LastIndex = PointIO->GetNum() - 1;
		Segmentation = new PCGExGeo::FSegmentation();
		Cloud = TypedContext->BoundsDataFacade->GetCloud();

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		if (Index == LastIndex) { return; }

		const int32 NextIndex = Index + 1;
		const FVector StartPosition = PointIO->GetInPoint(Index).Transform.GetLocation();
		const FVector EndPosition = PointIO->GetInPoint(NextIndex).Transform.GetLocation();

		PCGExGeo::FIntersections* Intersections = new PCGExGeo::FIntersections(
			StartPosition, EndPosition, Index, NextIndex);

		if (Cloud->FindIntersections(Intersections)) { Segmentation->Insert(Intersections); }
		else { PCGEX_DELETE(Intersections) }

		FPointsProcessor::ProcessSinglePoint(Index, Point, LoopIdx, LoopCount);
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		PCGExGeo::FIntersections* Intersections = Segmentation->IntersectionsList[Iteration];
		Intersections->Sort();

		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
		for (int i = 0; i < Intersections->Cuts.Num(); i++)
		{
			FPCGPoint& Point = MutablePoints[Intersections->Start + i];
			PCGExGeo::FCut& Cut = Intersections->Cuts[i];
			Point.Transform.SetLocation(Cut.Position);
		}
	}

	void FProcessor::CompleteWork()
	{
		const int32 NumCuts = Segmentation->GetNumCuts();
		if (NumCuts == 0)
		{
			// TODO : Add bool flags for intersect/inside as marks OR forward
			PointIO->InitializeOutput(PCGExData::EInit::Forward);
			return;
		}

		PointIO->InitializeOutput(PCGExData::EInit::NewOutput);
		const TArray<FPCGPoint>& OriginalPoints = PointIO->GetIn()->GetPoints();
		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();

		PCGEX_SET_NUM_UNINITIALIZED(MutablePoints, OriginalPoints.Num() + NumCuts);

		int32 Index = 0;
		MutablePoints[Index++] = OriginalPoints[0];
		for (int i = 1; i < OriginalPoints.Num(); i++)
		{
			const FPCGPoint& OriginalPoint = OriginalPoints[i];
			if (const PCGExGeo::FIntersections* Intersections = Segmentation->Find(PCGEx::H64U(i - 1, i)))
			{
				for (int j = 0; j < Intersections->Cuts.Num(); j++) { MutablePoints[Index++] = OriginalPoint; }
			}

			MutablePoints[Index++] = OriginalPoint;
		}

		//PointIO->InitializeNum(MutablePoints.Num(), true); // TODO : Embed in initial point loop

		Segmentation->ReduceToArray();
		StartParallelLoopForRange(Segmentation->IntersectionsList.Num());

		FPointsProcessor::CompleteWork();
	}

	void FProcessor::Write()
	{
		FPointsProcessor::Write();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
