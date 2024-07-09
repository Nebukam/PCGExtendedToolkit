// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPointsPathIntersection.h"

#define LOCTEXT_NAMESPACE "PCGExPointsPathIntersectionElement"
#define PCGEX_NAMESPACE PointsPathIntersection

TArray<FPCGPinProperties> UPCGExPointsPathIntersectionSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceTargetsLabel, "Intersection targets", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExPointsPathIntersectionSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(PointsPathIntersection)

FPCGExPointsPathIntersectionContext::~FPCGExPointsPathIntersectionContext()
{
	PCGEX_TERMINATE_ASYNC

	if (BoundsDataFacade) { PCGEX_DELETE(BoundsDataFacade->Source) }
	PCGEX_DELETE(BoundsDataFacade)
}

bool FPCGExPointsPathIntersectionElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PointsPathIntersection)

	PCGExData::FPointIO* BoundsIO = PCGExData::TryGetSingleInput(InContext, FName("Bounds"), true);
	if (!BoundsIO) { return false; }

	Context->BoundsDataFacade = new PCGExData::FFacade(BoundsIO);

	return true;
}

bool FPCGExPointsPathIntersectionElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPointsPathIntersectionElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PointsPathIntersection)

	PCGE_LOG(Error, GraphAndLog, FTEXT("NOT IMPLEMENTED YET"));
	return true;
}

namespace PCGExPathIntersections
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(Segmentation)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PointsPathIntersection)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LastIndex = PointIO->GetNum() - 1;
		Segmentation = new PCGExGeo::FSegmentation();
		Cloud = TypedContext->BoundsDataFacade->GetCloud();

		StartParallelLoopForPoints();

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

		int32 Index = 1;
		MutablePoints[0] = OriginalPoints[0];
		for (int i = 1; i < OriginalPoints.Num() - 1; i++)
		{
			if (const PCGExGeo::FIntersections* Intersections = Segmentation->Find(PCGEx::H64U(i - 1, i)))
			{
				Index += Intersections->Cuts.Num();
				for (int j = 0; j < Intersections->Cuts.Num(); j++) { MutablePoints[Index++] = OriginalPoints[i]; }
			}

			MutablePoints[Index++] = OriginalPoints[i];
		}

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
