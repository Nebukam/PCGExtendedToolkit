// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Splines/PCGExOrient.h"

#include "Splines/SubPoints/Orient/PCGExSubPointsOrientAverage.h"

#define LOCTEXT_NAMESPACE "PCGExOrientElement"

UPCGExOrientSettings::UPCGExOrientSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Orientation = EnsureInstruction<UPCGExSubPointsOrientAverage>(Orientation);
}

void UPCGExOrientSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Orientation = EnsureInstruction<UPCGExSubPointsOrientAverage>(Orientation);
	if (Orientation) { Orientation->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

FPCGElementPtr UPCGExOrientSettings::CreateElement() const { return MakeShared<FPCGExOrientElement>(); }

FPCGContext* FPCGExOrientElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExOrientContext* Context = new FPCGExOrientContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	const UPCGExOrientSettings* Settings = Context->GetInputSettings<UPCGExOrientSettings>();
	check(Settings);


	Context->Orientation = Settings->EnsureInstruction<UPCGExSubPointsOrientAverage>(Settings->Orientation, Context);

	return Context;
}


bool FPCGExOrientElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExOrientElement::Execute);

	FPCGExOrientContext* Context = static_cast<FPCGExOrientContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	Context->MainPoints->ForEach(
		[&](FPCGExPointIO& PointIO, int32)
		{
			Context->Orientation->PrepareForData(PointIO);
			Context->Orientation->ProcessPoints(PointIO.GetOut());
		});

	Context->OutputPoints();
	return true;
}

bool FOrientTask::ExecuteTask()
{
	return true;
}

#undef LOCTEXT_NAMESPACE
