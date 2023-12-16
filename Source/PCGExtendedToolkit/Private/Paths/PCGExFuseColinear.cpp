// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExFuseColinear.h"

#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExFuseColinearElement"
#define PCGEX_NAMESPACE FuseColinear

UPCGExFuseColinearSettings::UPCGExFuseColinearSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PCGEX_DEFAULT_OPERATION(Blending, UPCGExSubPointsBlendInterpolate)
}

void UPCGExFuseColinearSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Blending) { Blending->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

PCGExData::EInit UPCGExFuseColinearSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FPCGElementPtr UPCGExFuseColinearSettings::CreateElement() const { return MakeShared<FPCGExFuseColinearElement>(); }

FPCGExFuseColinearContext::~FPCGExFuseColinearContext()
{
}

PCGEX_INITIALIZE_CONTEXT(FuseColinear)

bool FPCGExFuseColinearElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FuseColinear)

	PCGEX_FWD(Threshold)
	PCGEX_FWD(bDoBlend)

	PCGEX_BIND_OPERATION(Blending, UPCGExSubPointsBlendInterpolate)

	return true;
}


bool FPCGExFuseColinearElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFuseColinearElement::Execute);

	PCGEX_CONTEXT(FuseColinear)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else { Context->SetState(PCGExMT::State_ProcessingPoints); }
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		const TArray<FPCGPoint>& InPoints = Context->CurrentIO->GetIn()->GetPoints();
		const int32 NumInPoints = InPoints.Num();

		for (int i = 0; i < NumInPoints; i++)
		{
		}

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
