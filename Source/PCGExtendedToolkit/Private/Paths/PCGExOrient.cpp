// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExOrient.h"

#include "Paths/SubPoints/Orient/PCGExSubPointsOrientAverage.h"

#define LOCTEXT_NAMESPACE "PCGExOrientElement"
#define PCGEX_NAMESPACE Orient

UPCGExOrientSettings::UPCGExOrientSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PCGEX_OPERATION_DEFAULT(Orientation, UPCGExSubPointsOrientAverage)
}

void UPCGExOrientSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Orientation) { Orientation->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

PCGEX_INITIALIZE_ELEMENT(Orient)

bool FPCGExOrientElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Orient)

	PCGEX_OPERATION_BIND(Orientation, UPCGExSubPointsOrientAverage)

	return true;
}

bool FPCGExOrientElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExOrientElement::Execute);

	PCGEX_CONTEXT(Orient)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	Context->MainPoints->ForEach(
		[&](PCGExData::FPointIO& PointIO, int32)
		{
			if (PointIO.GetNum() <= 1) { return; }
			Context->Orientation->PrepareForData(PointIO);
			Context->Orientation->ProcessPoints(PointIO.GetOut());
			Context->Output(PointIO);
		});

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
