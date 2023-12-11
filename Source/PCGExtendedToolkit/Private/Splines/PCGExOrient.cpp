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

FPCGContext* FPCGExOrientElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExOrientContext* Context = new FPCGExOrientContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	PCGEX_SETTINGS(UPCGExOrientSettings)

	PCGEX_BIND_OPERATION(Orientation, UPCGExSubPointsOrientAverage)

	return Context;
}


bool FPCGExOrientElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExOrientElement::Execute);

	PCGEX_CONTEXT(FPCGExOrientContext)

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
