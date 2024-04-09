// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExWritePathExtras.h"

#define LOCTEXT_NAMESPACE "PCGExWritePathExtrasElement"
#define PCGEX_NAMESPACE WritePathExtras

#if WITH_EDITOR
void UPCGExWritePathExtrasSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGExData::EInit UPCGExWritePathExtrasSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(WritePathExtras)

void UPCGExWritePathExtrasSettings::PostInitProperties()
{
	Super::PostInitProperties();
	PCGEX_OPERATION_DEFAULT(WritePathExtrasing, UPCGExMovingAverageWritePathExtrasing)
}

FPCGExWritePathExtrasContext::~FPCGExWritePathExtrasContext()
{
	PCGEX_TERMINATE_ASYNC
	PCGEX_FOREACH_FIELD_PATHEXTRAS(PCGEX_OUTPUT_DELETE)
}

bool FPCGExWritePathExtrasElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WritePathExtras)

	PCGEX_FOREACH_FIELD_PATHEXTRAS(PCGEX_OUTPUT_VALIDATE_NAME)
	PCGEX_FOREACH_FIELD_PATHEXTRAS(PCGEX_OUTPUT_FWD)

	PCGEX_FWD(bWritePathLength)
	PCGEX_FWD(bWritePathDirection)
	PCGEX_FWD(bWritePathCentroid)

	PCGEX_OUTPUT_VALIDATE_NAME_NOWRITER_SOFT(PathLength)
	PCGEX_OUTPUT_VALIDATE_NAME_NOWRITER_SOFT(PathDirection)
	PCGEX_OUTPUT_VALIDATE_NAME_NOWRITER_SOFT(PathCentroid)

	return true;
}

bool FPCGExWritePathExtrasElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWritePathExtrasElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(WritePathExtras)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (Context->CurrentIO->GetNum() < 2)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less that the than 2 points and will be discarded."));
				return false;
			}

			PCGExData::FPointIO& PointIO = *Context->CurrentIO;
			PointIO.InitializeOutput(PCGExData::EInit::DuplicateInput);

			PCGEX_FOREACH_FIELD_PATHEXTRAS(PCGEX_OUTPUT_ACCESSOR_INIT)

			Context->CurrentIO->CreateInKeys();
			Context->GetAsyncManager()->Start<FPCGExWritePathExtrasTask>(Context->CurrentPointIOIndex, Context->CurrentIO);

			Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
		}
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC
		PCGEX_FOREACH_FIELD_PATHEXTRAS(PCGEX_OUTPUT_WRITE)
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
	}

	return Context->IsDone();
}

bool FPCGExWritePathExtrasTask::ExecuteTask()
{
	const FPCGExWritePathExtrasContext* Context = Manager->GetContext<FPCGExWritePathExtrasContext>();
	PCGEX_SETTINGS(WritePathExtras)

	const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
	const int32 NumPoints = InPoints.Num();
	TArray<FVector> Positions;
	TArray<FVector> Normals;

	const FVector StaticUp = Settings->UpVector;
	PCGEx::FLocalVectorGetter* Up = new PCGEx::FLocalVectorGetter();

	if (Settings->bUseLocalUpVector)
	{
		Up->Capture(Settings->LocalUpVector);
		Up->Grab(*PointIO);
	}

	Positions.SetNum(NumPoints);
	Normals.SetNum(NumPoints);

	for (int i = 0; i < NumPoints; i++) { Positions[i] = InPoints[i].Transform.GetLocation(); }

	auto NRM = [&](const int32 A, const int32 B, const int32 C)-> FVector
	{
		const FVector VA = Positions[A];
		const FVector VB = Positions[B];
		const FVector VC = Positions[C];
		const FVector UpAverage = ((Up->SafeGet(A, StaticUp) + Up->SafeGet(B, StaticUp) + Up->SafeGet(C, StaticUp)) / 3).GetSafeNormal();
		return FMath::Lerp(PCGExMath::GetNormal(VA, VB, VB + UpAverage), PCGExMath::GetNormal(VB, VC, VC + UpAverage), 0.5).GetSafeNormal();
	};

	PCGExMath::FPathMetrics Metrics = PCGExMath::FPathMetrics(Positions[0]);

	const int32 LastIndex = NumPoints - 1;
	FVector PathCentroid = FVector::ZeroVector;

	PCGEX_OUTPUT_VALUE(DirectionToNext, 0, (Positions[0] - Positions[1]).GetSafeNormal());
	PCGEX_OUTPUT_VALUE(DirectionToPrev, 0, (Positions[1] - Positions[0]).GetSafeNormal());
	PCGEX_OUTPUT_VALUE(DistanceToStart, 0, 0);

	PCGEX_OUTPUT_VALUE(DistanceToNext, 0, FVector::Dist(Positions[0], Positions[1]));
	PCGEX_OUTPUT_VALUE(DistanceToPrev, 0, 0);

	FVector PathDir = (Positions[0] - Positions[1]);

	for (int i = 1; i < LastIndex; i++)
	{
		const double TraversedDistance = Metrics.Add(Positions[i]);
		PCGEX_OUTPUT_VALUE(PointNormal, i, NRM(i - 1, i, i + 1));
		PCGEX_OUTPUT_VALUE(DirectionToNext, i, (Positions[i] - Positions[i+1]).GetSafeNormal());
		PCGEX_OUTPUT_VALUE(DirectionToPrev, i, (Positions[i-1] - Positions[i]).GetSafeNormal());
		PCGEX_OUTPUT_VALUE(DistanceToStart, i, TraversedDistance);

		PCGEX_OUTPUT_VALUE(DistanceToNext, i, FVector::Dist(Positions[i],Positions[i+1]));
		PCGEX_OUTPUT_VALUE(DistanceToPrev, i, FVector::Dist(Positions[i-1],Positions[i]));

		PathDir += (Positions[i] - Positions[i + 1]);
	}

	Metrics.Add(Positions[LastIndex]);

	PCGEX_OUTPUT_VALUE(DirectionToNext, LastIndex, (Positions[LastIndex-1] - Positions[LastIndex]).GetSafeNormal());
	PCGEX_OUTPUT_VALUE(DirectionToPrev, LastIndex, (Positions[LastIndex] - Positions[LastIndex-1]).GetSafeNormal());
	PCGEX_OUTPUT_VALUE(DistanceToStart, LastIndex, Metrics.Length);

	PCGEX_OUTPUT_VALUE(DistanceToNext, LastIndex, 0);
	PCGEX_OUTPUT_VALUE(DistanceToPrev, LastIndex, FVector::Dist(Positions[LastIndex-1],Positions[LastIndex]));

	if (Settings->bClosedPath)
	{
		PCGEX_OUTPUT_VALUE(DirectionToPrev, 0, (Positions[0] - Positions[LastIndex]).GetSafeNormal());
		PCGEX_OUTPUT_VALUE(DirectionToNext, LastIndex, (Positions[LastIndex] - Positions[0]).GetSafeNormal());

		PCGEX_OUTPUT_VALUE(DistanceToNext, LastIndex, FVector::Dist(Positions[LastIndex], Positions[0]));
		PCGEX_OUTPUT_VALUE(DistanceToPrev, 0, FVector::Dist(Positions[0], Positions[LastIndex]));

		PCGEX_OUTPUT_VALUE(PointNormal, 0, NRM(LastIndex, 0, 1));
		PCGEX_OUTPUT_VALUE(PointNormal, LastIndex, NRM(NumPoints - 2, LastIndex, 0));
	}
	else
	{
		PCGEX_OUTPUT_VALUE(PointNormal, 0, NRM(0, 0, 1));
		PCGEX_OUTPUT_VALUE(PointNormal, LastIndex, NRM(NumPoints - 2, LastIndex, LastIndex));
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

	if (Context->bWritePathLength) { PCGExData::WriteMark(Meta, Settings->PathLengthAttributeName, Metrics.Length); }
	if (Context->bWritePathDirection) { PCGExData::WriteMark(Meta, Settings->PathDirectionAttributeName, (PathDir / NumPoints).GetSafeNormal()); }
	if (Context->bWritePathCentroid) { PCGExData::WriteMark(Meta, Settings->PathCentroidAttributeName, (PathCentroid / NumPoints).GetSafeNormal()); }

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
