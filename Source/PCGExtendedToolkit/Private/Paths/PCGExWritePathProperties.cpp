// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExWritePathProperties.h"

#include "PCGExDataMath.h"

#define LOCTEXT_NAMESPACE "PCGExWritePathPropertiesElement"
#define PCGEX_NAMESPACE WritePathProperties

PCGExData::EInit UPCGExWritePathPropertiesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(WritePathProperties)

FPCGExWritePathPropertiesContext::~FPCGExWritePathPropertiesContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExWritePathPropertiesElement::Boot(FPCGContext* InContext) const
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

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExWritePathProperties::FProcessor>>(
			[](PCGExData::FPointIO* Entry) { return Entry->GetNum() >= 2; },
			[&](PCGExPointsMT::TBatch<PCGExWritePathProperties::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any valid path."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->OutputMainPoints();

	return Context->TryComplete();
}

namespace PCGExWritePathProperties
{
	FProcessor::~FProcessor()
	{
		Positions.Empty();
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWritePathProperties::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WritePathProperties)

		bClosedPath = Settings->bClosedPath;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		{
			PCGExData::FFacade* OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_PATH(PCGEX_OUTPUT_INIT)
		}

		///

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		const int32 NumPoints = InPoints.Num();
		TArray<FVector> Normals;

		const FVector UpConstant = Settings->UpVector;
		const PCGExData::FCache<FVector>* UpGetter = Settings->bUseLocalUpVector ? PointDataFacade->GetBroadcaster<FVector>(Settings->LocalUpVector) : nullptr;

		Positions.SetNum(NumPoints);
		Normals.SetNum(NumPoints);

		bool bIsConvex = true;
		int32 Sign = 0;

		auto CheckConvex = [&](const int32 A, const int32 B, const int32 C)
		{
			if (!Settings->bTagConcave && !Settings->bTagConvex) { return; }
			PCGExMath::CheckConvex(
				Positions[A], Positions[B], Positions[C],
				bIsConvex, Sign);
		};

		for (int i = 0; i < NumPoints; i++) { Positions[i] = InPoints[i].Transform.GetLocation(); }

		PCGExMath::FPathMetrics Metrics = PCGExMath::FPathMetrics(Positions[0]);

		LastIndex = NumPoints - 1;
		FVector PathCentroid = FVector::ZeroVector;

		PCGEX_OUTPUT_VALUE(DirectionToNext, 0, (Positions[0] - Positions[1]).GetSafeNormal());
		PCGEX_OUTPUT_VALUE(DirectionToPrev, 0, (Positions[1] - Positions[0]).GetSafeNormal());
		PCGEX_OUTPUT_VALUE(DistanceToStart, 0, 0);

		PCGEX_OUTPUT_VALUE(DistanceToNext, 0, FVector::Dist(Positions[0], Positions[1]));
		PCGEX_OUTPUT_VALUE(DistanceToPrev, 0, 0);


		FVector PathDir = (Positions[0] - Positions[1]);

		for (int i = 1; i < LastIndex; i++)
		{
			CheckConvex(i - 1, i, i + 1);
			const double TraversedDistance = Metrics.Add(Positions[i]);
			PCGEX_OUTPUT_VALUE(PointNormal, i, PCGExMath::NRM(i - 1, i, i + 1, Positions, UpGetter, UpConstant));
			PCGEX_OUTPUT_VALUE(DirectionToNext, i, (Positions[i] - Positions[i + 1]).GetSafeNormal());
			PCGEX_OUTPUT_VALUE(DirectionToPrev, i, (Positions[i - 1] - Positions[i]).GetSafeNormal());
			PCGEX_OUTPUT_VALUE(DistanceToStart, i, TraversedDistance);

			PCGEX_OUTPUT_VALUE(DistanceToNext, i, FVector::Dist(Positions[i],Positions[i+1]));
			PCGEX_OUTPUT_VALUE(DistanceToPrev, i, FVector::Dist(Positions[i-1],Positions[i]));

			PathDir += (Positions[i] - Positions[i + 1]);
		}

		Metrics.Add(Positions[LastIndex]);

		PCGEX_OUTPUT_VALUE(DirectionToNext, LastIndex, (Positions[LastIndex - 1] - Positions[LastIndex]).GetSafeNormal());
		PCGEX_OUTPUT_VALUE(DirectionToPrev, LastIndex, (Positions[LastIndex] - Positions[LastIndex - 1]).GetSafeNormal());
		PCGEX_OUTPUT_VALUE(DistanceToStart, LastIndex, Metrics.Length);

		PCGEX_OUTPUT_VALUE(DistanceToNext, LastIndex, 0);
		PCGEX_OUTPUT_VALUE(DistanceToPrev, LastIndex, FVector::Dist(Positions[LastIndex-1],Positions[LastIndex]));

		if (bClosedPath)
		{
			CheckConvex(LastIndex, 0, 1);

			PCGEX_OUTPUT_VALUE(DirectionToPrev, 0, (Positions[0] - Positions[LastIndex]).GetSafeNormal());
			PCGEX_OUTPUT_VALUE(DirectionToNext, LastIndex, (Positions[LastIndex] - Positions[0]).GetSafeNormal());

			PCGEX_OUTPUT_VALUE(DistanceToNext, LastIndex, FVector::Dist(Positions[LastIndex], Positions[0]));
			PCGEX_OUTPUT_VALUE(DistanceToPrev, 0, FVector::Dist(Positions[0], Positions[LastIndex]));

			PCGEX_OUTPUT_VALUE(PointNormal, 0, PCGExMath::NRM(LastIndex, 0, 1, Positions, UpGetter, UpConstant));
			PCGEX_OUTPUT_VALUE(PointNormal, LastIndex, PCGExMath::NRM(NumPoints - 2, LastIndex, 0, Positions, UpGetter, UpConstant));
		}
		else
		{
			PCGEX_OUTPUT_VALUE(PointNormal, 0, PCGExMath::NRM(0, 0, 1, Positions, UpGetter, UpConstant));
			PCGEX_OUTPUT_VALUE(PointNormal, LastIndex, PCGExMath::NRM(NumPoints - 2, LastIndex, LastIndex, Positions, UpGetter, UpConstant));
		}

		PCGExMath::FPathMetrics SecondMetrics = PCGExMath::FPathMetrics(Positions[0]);

		for (int i = 0; i < NumPoints; i++)
		{
			const double TraversedDistance = SecondMetrics.Add(Positions[i]);
			PCGEX_OUTPUT_VALUE(PointTime, i, TraversedDistance / Metrics.Length);
			PCGEX_OUTPUT_VALUE(DistanceToEnd, i, Metrics.Length - TraversedDistance);
			PathCentroid += Positions[i];
		}

		UPCGMetadata* Meta = PointIO->GetOut()->Metadata;

		if (TypedContext->bWritePathLength) { PCGExData::WriteMark(Meta, Settings->PathLengthAttributeName, Metrics.Length); }
		if (TypedContext->bWritePathDirection) { PCGExData::WriteMark(Meta, Settings->PathDirectionAttributeName, (PathDir / NumPoints).GetSafeNormal()); }
		if (TypedContext->bWritePathCentroid) { PCGExData::WriteMark(Meta, Settings->PathCentroidAttributeName, (PathCentroid / NumPoints).GetSafeNormal()); }

		///

		if (Settings->bWriteDot) { StartParallelLoopForPoints(); }

		if (Sign != 0)
		{
			if (Settings->bTagConcave && !bIsConvex) { PointIO->Tags->RawTags.Add(Settings->ConcaveTag); }
			if (Settings->bTagConvex && bIsConvex) { PointIO->Tags->RawTags.Add(Settings->ConvexTag); }
		}

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		FVector Loc = Point.Transform.GetLocation();
		FVector Next;
		FVector Prev;

		if (Index == 0)
		{
			Prev = bClosedPath ? Positions[LastIndex] : FVector::ZeroVector;
			Next = Positions[Index + 1];
		}
		else if (Index == LastIndex)
		{
			Prev = Positions[Index - 1];
			Next = bClosedPath ? Positions[0] : FVector::ZeroVector;
		}
		else
		{
			Prev = Positions[Index - 1];
			Next = Positions[Index + 1];
		}

		PCGEX_OUTPUT_VALUE(Dot, Index, FVector::DotProduct((Prev - Loc).GetSafeNormal(), (Loc - Next).GetSafeNormal()));
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
