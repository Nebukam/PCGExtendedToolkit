// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExDiscardByPointCount.h"

#include "PCGExPointsProcessor.h"

#define LOCTEXT_NAMESPACE "PCGExDiscardByPointCountElement"
#define PCGEX_NAMESPACE DiscardByPointCount

PCGExData::EInit UPCGExDiscardByPointCountSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FPCGElementPtr UPCGExDiscardByPointCountSettings::CreateElement() const { return MakeShared<FPCGExDiscardByPointCountElement>(); }

bool FPCGExDiscardByPointCountElement::Boot(FPCGExContext* InContext) const
{
	FPCGExPointsProcessorContext* Context = static_cast<FPCGExPointsProcessorContext*>(InContext);
	PCGEX_SETTINGS(PointsProcessor)

	Context->MainPoints = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->MainPoints->DefaultOutputLabel = Settings->GetMainOutputLabel();

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(Settings->GetMainInputLabel());
	Context->MainPoints->Initialize(Sources, Settings->GetMainOutputInitMode());

	return true;
}

bool FPCGExDiscardByPointCountElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDiscardByPointCountElement::Execute);

	PCGEX_CONTEXT(PointsProcessor)
	PCGEX_SETTINGS(DiscardByPointCount)
	PCGEX_ON_INITIAL_EXECUTION
	{
		const int32 Min = (Settings->bRemoveBelow && Settings->MinPointCount >= 0) ? Settings->MinPointCount : -1;
		const int32 Max = (Settings->bRemoveAbove && Settings->MaxPointCount >= 0) ? Settings->MaxPointCount : TNumericLimits<int32>::Max();

		for (const TSharedPtr<PCGExData::FPointIO>& PointIO : Context->MainPoints->Pairs)
		{
			if (!FMath::IsWithin(PointIO->GetNum(), Min, Max)) { continue; }
			PointIO->InitializeOutput(Context, PCGExData::EInit::Forward);
		}

		Context->MainPoints->StageOutputs();
		Context->Done();
	}

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
