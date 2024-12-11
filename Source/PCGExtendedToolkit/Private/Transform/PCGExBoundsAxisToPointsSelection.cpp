// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExBoundsAxisToPointsSelection.h"


#define LOCTEXT_NAMESPACE "PCGExBoundsAxisToPointsElement"
#define PCGEX_NAMESPACE BoundsAxisToPoints

PCGExData::EIOInit UPCGExBoundsAxisToPointsSettings::GetMainOutputInitMode() const { return bGeneratePerPointData ? PCGExData::EIOInit::None : PCGExData::EIOInit::Duplicate; }

PCGEX_INITIALIZE_ELEMENT(BoundsAxisToPoints)

bool FPCGExBoundsAxisToPointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BoundsAxisToPoints)

	return true;
}

bool FPCGExBoundsAxisToPointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBoundsAxisToPointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BoundsAxisToPoints)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBoundsAxisToPoints::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExBoundsAxisToPoints::FProcessor>>& NewBatch)
			{
				//NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Missing data."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExBoundsAxisToPoints
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBoundsAxisToPoints::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		bSetExtents = Settings->bSetExtents;
		Extents = Settings->Extents;

		bSetScale = Settings->bSetScale;
		Scale = Settings->Scale;

		PointAttributesToOutputTags = Settings->PointAttributesToOutputTags;
		if (!PointAttributesToOutputTags.Init(ExecutionContext, PointDataFacade)) { return false; }

		NumPoints = PointDataFacade->GetNum();
		bGeneratePerPointData = Settings->bGeneratePerPointData;

		if (bGeneratePerPointData)
		{
			NewOutputs.SetNum(PointDataFacade->GetNum());
			for (int i = 0; i < NewOutputs.Num(); i++)
			{
				NewOutputs[i] = Context->MainPoints->Emplace_GetRef(PointDataFacade->Source, PCGExData::EIOInit::New);
			}
		}
		else
		{
			PointDataFacade->GetOut()->GetMutablePoints().SetNumUninitialized(NumPoints * 2);
		}

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		const FVector E = PCGExMath::GetLocalBounds(Point, Settings->BoundsReference).GetExtent();
		FPCGExUVW UVW;
		UVW.BoundsReference = Settings->BoundsReference;

		EPCGExMinimalAxis Axis = EPCGExMinimalAxis::None;

		switch (Settings->Selection)
		{
		case EPCGExBoundsAxisToPointsSelection::Shortest:
			if (E.X < E.Y && E.X < E.Z) { Axis = EPCGExMinimalAxis::X; }
			else if (E.Y < E.X && E.Y < E.Z) { Axis = EPCGExMinimalAxis::Y; }
			else { Axis = EPCGExMinimalAxis::Z; }
			break;
		case EPCGExBoundsAxisToPointsSelection::NextShortest:
			if (E.X < E.Y && E.X < E.Z)
			{
				if (E.Y < E.Z) { Axis = EPCGExMinimalAxis::Y; }
				else { Axis = EPCGExMinimalAxis::Z; }
			}
			else if (E.Y < E.X && E.Y < E.Z)
			{
				if (E.X < E.Z) { Axis = EPCGExMinimalAxis::X; }
				else { Axis = EPCGExMinimalAxis::Z; }
			}
			else
			{
				if (E.X < E.Y) { Axis = EPCGExMinimalAxis::X; }
				else { Axis = EPCGExMinimalAxis::Y; }
			}
			break;
		case EPCGExBoundsAxisToPointsSelection::Longest:
			if (E.X > E.Y && E.X > E.Z) { Axis = EPCGExMinimalAxis::X; }
			else if (E.Y > E.X && E.Y > E.Z) { Axis = EPCGExMinimalAxis::Y; }
			else { Axis = EPCGExMinimalAxis::Z; }
			break;
		case EPCGExBoundsAxisToPointsSelection::NextLongest:
			if (E.X > E.Y && E.X > E.Z)
			{
				if (E.Y > E.Z) { Axis = EPCGExMinimalAxis::Y; }
				else { Axis = EPCGExMinimalAxis::Z; }
			}
			else if (E.Y > E.X && E.Y > E.Z)
			{
				if (E.X > E.Z) { Axis = EPCGExMinimalAxis::X; }
				else { Axis = EPCGExMinimalAxis::Z; }
			}
			else
			{
				if (E.X > E.Y) { Axis = EPCGExMinimalAxis::X; }
				else { Axis = EPCGExMinimalAxis::Y; }
			}
			break;
		case EPCGExBoundsAxisToPointsSelection::ShortestAbove:
			if (E.X > Settings->Threshold && E.X < E.Y && E.X < E.Z) { Axis = EPCGExMinimalAxis::X; }
			else if (E.Y > Settings->Threshold && E.Y < E.X && E.Y < E.Z) { Axis = EPCGExMinimalAxis::Y; }
			else if (E.Z > Settings->Threshold) { Axis = EPCGExMinimalAxis::Z; }
			else
			{
				if (E.X < E.Y && E.X < E.Z) { Axis = EPCGExMinimalAxis::X; }
				else if (E.Y < E.X && E.Y < E.Z) { Axis = EPCGExMinimalAxis::Y; }
				else { Axis = EPCGExMinimalAxis::Z; }
			}
			break;
		}

		switch (Axis)
		{
		case EPCGExMinimalAxis::None:
		case EPCGExMinimalAxis::X:
			UVW.UConstant = Settings->U;
			break;
		case EPCGExMinimalAxis::Y:
			UVW.VConstant = Settings->U;
			break;
		case EPCGExMinimalAxis::Z:
			UVW.WConstant = Settings->U;
			break;
		}

		if (bGeneratePerPointData)
		{
			int32 OutIndex;
			const TSharedPtr<PCGExData::FPointIO>& NewOutput = NewOutputs[Index];

			FPCGPoint& A = NewOutput->CopyPoint(Point, OutIndex);
			if (bSetExtents)
			{
				A.BoundsMin = -Extents;
				A.BoundsMax = Extents;
			}

			A.Transform.SetLocation(UVW.GetPosition(PointIO->GetInPointRef(Index)));
			if (bSetScale) { A.Transform.SetScale3D(Scale); }

			FPCGPoint& B = NewOutput->CopyPoint(Point, OutIndex);
			if (bSetExtents)
			{
				B.BoundsMin = -Extents;
				B.BoundsMax = Extents;
			}

			B.Transform.SetLocation(UVW.GetPosition(PointIO->GetInPointRef(Index), Axis, true));
			if (bSetScale) { B.Transform.SetScale3D(Scale); }


			PointAttributesToOutputTags.Tag(Index, NewOutput);
		}
		else
		{
			TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();

			FPCGPoint& A = MutablePoints[Index];
			if (bSetExtents)
			{
				A.BoundsMin = -Extents;
				A.BoundsMax = Extents;
			}

			A.Transform.SetLocation(UVW.GetPosition(PointIO->GetInPointRef(Index)));
			if (bSetScale) { A.Transform.SetScale3D(Scale); }

			MutablePoints[NumPoints + Index] = Point;
			FPCGPoint& B = MutablePoints[NumPoints + Index];
			if (bSetExtents)
			{
				B.BoundsMin = -Extents;
				B.BoundsMax = Extents;
			}

			B.Transform.SetLocation(UVW.GetPosition(PointIO->GetInPointRef(Index), Axis, true));
			if (bSetScale) { B.Transform.SetScale3D(Scale); }
		}
	}

	void FProcessor::CompleteWork()
	{
		if (!bGeneratePerPointData)
		{
			TArray<FPCGPoint>& MutablePoints = PointDataFacade->GetOut()->GetMutablePoints();
			UPCGMetadata* Metadata = PointDataFacade->GetOut()->Metadata;
			for (int i = NumPoints; i < MutablePoints.Num(); i++) { Metadata->InitializeOnSet(MutablePoints[i].MetadataEntry); }
		}
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
