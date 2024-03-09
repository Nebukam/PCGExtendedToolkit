// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExSmooth.h"

#include "Paths/Smoothing/PCGExMovingAverageSmoothing.h"

#define LOCTEXT_NAMESPACE "PCGExSmoothElement"
#define PCGEX_NAMESPACE Smooth

#if WITH_EDITOR
void UPCGExSmoothSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Smoothing) { Smoothing->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGExData::EInit UPCGExSmoothSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(Smooth)

void UPCGExSmoothSettings::PostInitProperties()
{
	Super::PostInitProperties();
	PCGEX_OPERATION_DEFAULT(Smoothing, UPCGExMovingAverageSmoothing)
}

FPCGExSmoothContext::~FPCGExSmoothContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExSmoothElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Smooth)

	PCGEX_OPERATION_BIND(Smoothing, UPCGExMovingAverageSmoothing)

	Context->Smoothing->bClosedPath = Settings->bClosedPath;
	Context->Smoothing->bPreserveStart = Settings->bPreserveStart;
	Context->Smoothing->bPreserveEnd = Settings->bPreserveEnd;
	Context->Smoothing->FixedInfluence = Settings->Influence;
	Context->Smoothing->bUseLocalInfluence = Settings->bUseLocalInfluence;
	Context->Smoothing->InfluenceDescriptor = FPCGExInputDescriptor(Settings->LocalInfluence);

	return true;
}

bool FPCGExSmoothElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSmoothElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(Smooth)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		int32 Index = 0;
		Context->MainPoints->ForEach(
			[&](PCGExData::FPointIO& PointIO, const int32)
			{
				if (PointIO.GetNum() > 2)
				{
					PointIO.CreateInKeys();
					PointIO.CreateOutKeys();
					Context->GetAsyncManager()->Start<FPCGExSmoothTask>(Index++, &PointIO);
				}
			});
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC
		Context->OutputPoints();
		Context->Done();
	}

	return Context->IsDone();
}

bool FPCGExSmoothTask::ExecuteTask()
{
	const FPCGExSmoothContext* Context = Manager->GetContext<FPCGExSmoothContext>();
	Context->Smoothing->DoSmooth(*PointIO);
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
