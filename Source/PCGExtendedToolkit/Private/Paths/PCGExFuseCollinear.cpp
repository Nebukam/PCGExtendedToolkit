// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExFuseCollinear.h"

#include "Data/PCGExDataFilter.h"

#define LOCTEXT_NAMESPACE "PCGExFuseCollinearElement"
#define PCGEX_NAMESPACE FuseCollinear

UPCGExFuseCollinearSettings::UPCGExFuseCollinearSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//PCGEX_OPERATION_DEFAULT(Blending, UPCGExSubPointsBlendInterpolate)
}

#if WITH_EDITOR
void UPCGExFuseCollinearSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//if (Blending) { Blending->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGExData::EInit UPCGExFuseCollinearSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FName UPCGExFuseCollinearSettings::GetPointFilterLabel() const { return PCGExDataFilter::SourceFiltersLabel; }

PCGEX_INITIALIZE_ELEMENT(FuseCollinear)

FPCGExFuseCollinearContext::~FPCGExFuseCollinearContext()
{
	PCGEX_TERMINATE_ASYNC
}


bool FPCGExFuseCollinearElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FuseCollinear)

	Context->Threshold = PCGExMath::DegreesToDot(Settings->Threshold);

	Context->FuseDistance = Settings->FuseDistance * Settings->FuseDistance;

	//PCGEX_FWD(bDoBlend)
	//PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInterpolate)

	return true;
}


bool FPCGExFuseCollinearElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFuseCollinearElement::Execute);

	PCGEX_CONTEXT(FuseCollinear)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (!Context->ExecuteAutomation()) { return false; }

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->Done();
			return false;
		}

		Context->SetState(PCGExMT::State_ProcessingPoints);
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		if (Context->CurrentIO->GetNum() <= 2)
		{
			Context->CurrentIO->InitializeOutput(PCGExData::EInit::DuplicateInput);
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		Context->GetAsyncManager()->Start<FPCGExFuseCollinearTask>(Context->CurrentIO->IOIndex, Context->CurrentIO);
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputMainPoints();
		Context->ExecuteEnd();
	}

	return Context->IsDone();
}

bool FPCGExFuseCollinearTask::ExecuteTask()
{
	const FPCGExFuseCollinearContext* Context = Manager->GetContext<FPCGExFuseCollinearContext>();
	PCGEX_SETTINGS(FuseCollinear)

	const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
	TArray<FPCGPoint>& OutPoints = PointIO->GetOut()->GetMutablePoints();
	OutPoints.Add_GetRef(InPoints[0]);

	FVector LastPosition = InPoints[0].Transform.GetLocation();
	FVector CurrentDirection = (LastPosition - InPoints[1].Transform.GetLocation()).GetSafeNormal();
	const int32 MaxIndex = InPoints.Num() - 1;

	for (int i = 1; i < MaxIndex; i++)
	{
		FVector CurrentPosition = InPoints[i].Transform.GetLocation();
		FVector NextPosition = InPoints[i + 1].Transform.GetLocation();
		FVector DirToNext = (CurrentPosition - NextPosition).GetSafeNormal();

		const double Dot = FVector::DotProduct(CurrentDirection, DirToNext);
		const bool bWithinThreshold = Settings->bInvertThreshold ? Dot < Context->Threshold : Dot > Context->Threshold;
		if (FVector::DistSquared(CurrentPosition, LastPosition) <= Context->FuseDistance || bWithinThreshold)
		{
			// Collinear with previous, keep moving
			continue;
		}

		OutPoints.Add_GetRef(InPoints[i]);
		CurrentDirection = DirToNext;
		LastPosition = CurrentPosition;
	}

	OutPoints.Add_GetRef(InPoints[MaxIndex]);

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
