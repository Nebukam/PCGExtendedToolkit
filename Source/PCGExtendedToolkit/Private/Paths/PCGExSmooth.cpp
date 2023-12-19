// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExSmooth.h"

#include "Paths/Smoothing/PCGExMovingAverageSmoothing.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExSmoothElement"
#define PCGEX_NAMESPACE Smooth

UPCGExSmoothSettings::UPCGExSmoothSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PCGEX_DEFAULT_OPERATION(Smoothing, UPCGExMovingAverageSmoothing)
}

void UPCGExSmoothSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Smoothing) { Smoothing->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

PCGExData::EInit UPCGExSmoothSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FPCGElementPtr UPCGExSmoothSettings::CreateElement() const { return MakeShared<FPCGExSmoothElement>(); }

FPCGExSmoothContext::~FPCGExSmoothContext()
{
	PCGEX_TERMINATE_ASYNC
}

PCGEX_INITIALIZE_CONTEXT(Smooth)

bool FPCGExSmoothElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Smooth)

	PCGEX_BIND_OPERATION(Smoothing, UPCGExMovingAverageSmoothing)
	Context->Smoothing->bPinStart = Settings->bPinStart;
	Context->Smoothing->bPinEnd = Settings->bPinEnd;
	Context->Smoothing->FixedInfluence = Settings->Influence;
	Context->Smoothing->bUseLocalInfluence = Settings->bUseLocalInfluence;
	Context->Smoothing->InfluenceDescriptor = Settings->LocalInfluence;

	return true;
}

bool FPCGExSmoothElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSmoothElement::Execute);

	PCGEX_CONTEXT(Smooth)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		int32 Index = 0;
		while (Context->AdvancePointsIO())
		{
			if (Context->CurrentIO->GetNum() > 2)
			{
				Context->CurrentIO->CreateInKeys();
				Context->CurrentIO->CreateOutKeys();
				Context->GetAsyncManager()->Start<FSmoothTask>(Index++, Context->CurrentIO);
			}
		}
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (Context->IsAsyncWorkComplete())
		{
			Context->OutputPoints();
			Context->Done();
		}
	}

	return Context->IsDone();
}

bool FSmoothTask::ExecuteTask()
{
	const FPCGExSmoothContext* Context = Manager->GetContext<FPCGExSmoothContext>();
	Context->Smoothing->DoSmooth(*PointIO);
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
