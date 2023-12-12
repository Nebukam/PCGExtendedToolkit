// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Splines/PCGExOrient.h"

#include "Splines/SubPoints/Orient/PCGExSubPointsOrientAverage.h"

#define LOCTEXT_NAMESPACE "PCGExOrientElement"

UPCGExOrientSettings::UPCGExOrientSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Orientation = EnsureOperation<UPCGExSubPointsOrientAverage>(Orientation);
}

void UPCGExOrientSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Orientation = EnsureOperation<UPCGExSubPointsOrientAverage>(Orientation);
	if (Orientation) { Orientation->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

FPCGElementPtr UPCGExOrientSettings::CreateElement() const { return MakeShared<FPCGExOrientElement>(); }

PCGEX_INITIALIZE_CONTEXT(Orient)

bool FPCGExOrientElement::Validate(FPCGContext* InContext) const
{
	if(! FPCGExPathProcessorElement::Validate(InContext)){return false;}
	
	PCGEX_CONTEXT_AND_SETTINGS(Orient)

	PCGEX_BIND_OPERATION(Orientation, UPCGExSubPointsOrientAverage)

	return true;
}


bool FPCGExOrientElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExOrientElement::Execute);

	PCGEX_CONTEXT(Orient)

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
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
