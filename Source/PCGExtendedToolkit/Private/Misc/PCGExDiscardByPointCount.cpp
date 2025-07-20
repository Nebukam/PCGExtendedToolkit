// Copyright 2025 Timothé Lapetite and contributors
// * 24/02/25 Omer Salomon Fixed - Made the count condition for discarding to be inclusive on MaxPointCount (to fit the comment).
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExDiscardByPointCount.h"

#include "PCGExPointsProcessor.h"

#define LOCTEXT_NAMESPACE "PCGExDiscardByPointCountElement"
#define PCGEX_NAMESPACE DiscardByPointCount

TArray<FPCGPinProperties> UPCGExDiscardByPointCountSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExDiscardByPointCount::OutputDiscardedLabel, "Discarded outputs.", Normal, {})
	return PinProperties;
}

FPCGElementPtr UPCGExDiscardByPointCountSettings::CreateElement() const { return MakeShared<FPCGExDiscardByPointCountElement>(); }

bool FPCGExDiscardByPointCountElement::Boot(FPCGExContext* InContext) const
{
	FPCGExPointsProcessorContext* Context = static_cast<FPCGExPointsProcessorContext*>(InContext);
	PCGEX_SETTINGS(PointsProcessor)

	Context->MainPoints = MakeShared<PCGExData::FPointIOCollection>(Context);

	Context->MainPoints->OutputPin = Settings->GetMainOutputPin();

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(Settings->GetMainInputPin());
	Context->MainPoints->Initialize(Sources, PCGExData::EIOInit::Forward);

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
		const int32 Max = (Settings->bRemoveAbove && Settings->MaxPointCount >= 0) ? Settings->MaxPointCount : MAX_int32;

		const int32 NumTotal = Context->MainPoints->Num();
		int32 NumDiscarded = 0;

		for (const TSharedPtr<PCGExData::FPointIO>& PointIO : Context->MainPoints->Pairs)
		{
			PointIO->bAllowEmptyOutput = Settings->bAllowEmptyOutputs;
			if (!FMath::IsWithinInclusive(PointIO->GetNum(), Min, Max))
			{
				PointIO->OutputPin = PCGExDiscardByPointCount::OutputDiscardedLabel;
				NumDiscarded++;
			}
		}

		Context->MainPoints->StageOutputs();
		Context->Done();

		if (NumDiscarded == NumTotal) { Context->OutputData.InactiveOutputPinBitmask = 1; }
		else if (NumDiscarded == 0) { Context->OutputData.InactiveOutputPinBitmask = 2; }
	}

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
