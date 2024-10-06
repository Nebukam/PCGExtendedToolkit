// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExBoundsToPoints.h"


#define LOCTEXT_NAMESPACE "PCGExBoundsToPointsElement"
#define PCGEX_NAMESPACE BoundsToPoints

PCGExData::EInit UPCGExBoundsToPointsSettings::GetMainOutputInitMode() const { return bGeneratePerPointData ? PCGExData::EInit::NoOutput : PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(BoundsToPoints)

bool FPCGExBoundsToPointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BoundsToPoints)

	return true;
}

bool FPCGExBoundsToPointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBoundsToPointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BoundsToPoints)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBoundsToPoints::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExBoundsToPoints::FProcessor>>& NewBatch)
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

namespace PCGExBoundsToPoints
{
	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBoundsToPoints::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		bSetExtents = Settings->bSetExtents;
		Extents = Settings->Extents;

		bSetScale = Settings->bSetScale;
		Scale = Settings->Scale;

		Axis = Settings->SymmetryAxis;
		UVW = Settings->UVW;
		if (!UVW.Init(ExecutionContext, PointDataFacade)) { return false; }

		PointAttributesToOutputTags = Settings->PointAttributesToOutputTags;
		if (!PointAttributesToOutputTags.Init(ExecutionContext, PointDataFacade)) { return false; }

		NumPoints = PointDataFacade->GetNum();
		bGeneratePerPointData = Settings->bGeneratePerPointData;
		bSymmetry = Settings->SymmetryAxis != EPCGExMinimalAxis::None;

		if (bGeneratePerPointData)
		{
			NewOutputs.SetNum(PointDataFacade->GetNum());
			for (int i = 0; i < NewOutputs.Num(); i++)
			{
				NewOutputs[i] = Context->MainPoints->Emplace_GetRef(PointDataFacade->Source, PCGExData::EInit::NewOutput);
			}

			if (bSymmetry)
			{
			}
			else
			{
			}
		}
		else
		{
			if (bSymmetry)
			{
				PointDataFacade->GetOut()->GetMutablePoints().SetNumUninitialized(NumPoints * 2);
			}
			else
			{
			}
		}

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

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

			if (bSymmetry)
			{
				FPCGPoint& B = NewOutput->CopyPoint(Point, OutIndex);
				if (bSetExtents)
				{
					B.BoundsMin = -Extents;
					B.BoundsMax = Extents;
				}

				B.Transform.SetLocation(UVW.GetPosition(PointIO->GetInPointRef(Index), Axis, true));
				if (bSetScale) { B.Transform.SetScale3D(Scale); }
			}

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

			if (bSymmetry)
			{
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
	}

	void FProcessor::CompleteWork()
	{
		if (!bGeneratePerPointData && bSymmetry)
		{
			TArray<FPCGPoint>& MutablePoints = PointDataFacade->GetOut()->GetMutablePoints();
			UPCGMetadata* Metadata = PointDataFacade->GetOut()->Metadata;
			for (int i = NumPoints; i < MutablePoints.Num(); i++) { Metadata->InitializeOnSet(MutablePoints[i].MetadataEntry); }
		}
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
