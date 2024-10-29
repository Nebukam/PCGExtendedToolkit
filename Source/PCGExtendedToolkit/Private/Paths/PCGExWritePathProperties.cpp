// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExWritePathProperties.h"

#include "PCGExDataMath.h"


#define LOCTEXT_NAMESPACE "PCGExWritePathPropertiesElement"
#define PCGEX_NAMESPACE WritePathProperties

PCGExData::EInit UPCGExWritePathPropertiesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(WritePathProperties)

bool FPCGExWritePathPropertiesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WritePathProperties)

	PCGEX_FOREACH_FIELD_PATH(PCGEX_OUTPUT_VALIDATE_NAME)
	PCGEX_FOREACH_FIELD_PATH_MARKS(PCGEX_OUTPUT_VALIDATE_NAME)

	return true;
}

bool FPCGExWritePathPropertiesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWritePathPropertiesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(WritePathProperties)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExWritePathProperties::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return Entry->GetNum() >= 2; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExWritePathProperties::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any valid path."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExWritePathProperties
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWritePathProperties::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		bClosedLoop = Context->ClosedLoop.IsClosedLoop(PointIO);
		Path = PCGExPaths::MakePath(PointDataFacade->GetIn()->GetPoints(), 0, bClosedLoop);
		Path->IOIndex = PointDataFacade->Source->IOIndex;
		PathLength = Path->AddExtra<PCGExPaths::FPathEdgeLength>(true); // Force compute length
		if (Settings->bWritePointNormal || Settings->bWritePointBinormal) { PathBinormal = Path->AddExtra<PCGExPaths::FPathEdgeBinormal>(false, Settings->UpVector); }
		if (Settings->bWritePointAvgNormal) { PathAvgNormal = Path->AddExtra<PCGExPaths::FPathEdgeAvgNormal>(false, Settings->UpVector); }

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_PATH(PCGEX_OUTPUT_INIT)
		}

		///

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		const int32 NumPoints = InPoints.Num();

		PCGEx::InitArray(Details, NumPoints);

		for (int i = 0; i < NumPoints; i++)
		{
			Details[i] = {
				i,
				FVector::ZeroVector,
				FVector::ZeroVector,
				FVector::ZeroVector,
				FVector::ZeroVector
			};
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		FPointDetails& Current = Details[Index];

		Current.ToPrev = Path->DirToPrevPoint(Index);
		Current.ToNext = Path->DirToNextPoint(Index);

		int32 ExtraIndex = !bClosedLoop && Index == Path->LastIndex ? Path->LastEdge : Index;
		Path->ComputeEdgeExtra(ExtraIndex);
		
		PCGEX_OUTPUT_VALUE(PointNormal, Index, PathBinormal->Normals[ExtraIndex]);
		PCGEX_OUTPUT_VALUE(PointBinormal, Index, PathBinormal->Get(ExtraIndex));
		PCGEX_OUTPUT_VALUE(PointAvgNormal, Index, PathAvgNormal->Get(ExtraIndex));

		PCGEX_OUTPUT_VALUE(DirectionToNext, Index, Current.ToNext);
		PCGEX_OUTPUT_VALUE(DirectionToPrev, Index, Current.ToPrev);

		PCGEX_OUTPUT_VALUE(DistanceToNext, Index, !Path->IsClosedLoop() && Index == Path->LastIndex ? 0 : PathLength->Get(Index))
		PCGEX_OUTPUT_VALUE(DistanceToPrev, Index, Index == 0 ? Path->IsClosedLoop() ? PathLength->Get(Path->LastEdge) : 0 : PathLength->Get(Index-1))

		PCGEX_OUTPUT_VALUE(Dot, Index, FVector::DotProduct(Current.ToPrev, Current.ToNext));
		PCGEX_OUTPUT_VALUE(Angle, Index, PCGExSampling::GetAngle(Settings->AngleRange, Current.ToPrev, Current.ToNext));
	}

	void FProcessor::CompleteWork()
	{
		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		FVector PathCentroid = FVector::ZeroVector;
		FVector PathDir = Details[0].ToNext;

		// Compute path-wide data

		// Compute path-wide, per-point stuff
		double TraversedDistance = 0;
		for (int i = 0; i < Path->NumPoints; i++)
		{
			if (Settings->bTagConcave || Settings->bTagConvex) { Path->UpdateConvexity(i); }

			const FPointDetails& Detail = Details[i];
			PathDir += Detail.ToNext;

			PCGEX_OUTPUT_VALUE(PointTime, i, TraversedDistance / PathLength->TotalLength);

			PCGEX_OUTPUT_VALUE(DistanceToStart, i, TraversedDistance);
			PCGEX_OUTPUT_VALUE(DistanceToEnd, i, PathLength->TotalLength - TraversedDistance);

			TraversedDistance += !Path->IsClosedLoop() && i == Path->LastIndex ? 0 : PathLength->Get(i);
			PathCentroid += Path->GetPosUnsafe(i);
		}

		if (!bClosedLoop)
		{
			const FPointDetails& First = Details[0];
			const FPointDetails& Last = Details[Path->LastIndex];

			PCGEX_OUTPUT_VALUE(Dot, 0, -1);
			PCGEX_OUTPUT_VALUE(Angle, 0, PCGExSampling::GetAngle(Settings->AngleRange, First.ToNext *-1, First.ToNext));

			PCGEX_OUTPUT_VALUE(Dot, Path->LastIndex, -1);
			PCGEX_OUTPUT_VALUE(Angle, Path->LastIndex, PCGExSampling::GetAngle(Settings->AngleRange, Last.ToPrev *-1, Last.ToPrev));
		}

		if (Context->bWritePathLength) { WriteMark(PointIO, Settings->PathLengthAttributeName, PathLength->TotalLength); }
		if (Context->bWritePathDirection) { WriteMark(PointIO, Settings->PathDirectionAttributeName, (PathDir / Path->NumPoints).GetSafeNormal()); }
		if (Context->bWritePathCentroid) { WriteMark(PointIO, Settings->PathCentroidAttributeName, (PathCentroid / Path->NumPoints).GetSafeNormal()); }

		///

		if (Path->ConvexitySign != 0)
		{
			if (Settings->bTagConcave && !Path->bIsConvex) { PointIO->Tags->Add(Settings->ConcaveTag); }
			if (Settings->bTagConvex && Path->bIsConvex) { PointIO->Tags->Add(Settings->ConvexTag); }
		}

		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
