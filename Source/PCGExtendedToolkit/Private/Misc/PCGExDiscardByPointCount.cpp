// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExDiscardByPointCount.h"

#include "PCGExPointsProcessor.h"

#define LOCTEXT_NAMESPACE "PCGExDiscardByPointCountElement"
#define PCGEX_NAMESPACE DiscardByPointCount

PCGExData::EIOInit UPCGExDiscardByPointCountSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Forward; }

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
		const int32 Max = (Settings->bRemoveAbove && Settings->MaxPointCount >= 0) ? Settings->MaxPointCount : MAX_int32;

		for (const TSharedPtr<PCGExData::FPointIO>& PointIO : Context->MainPoints->Pairs)
		{
			PointIO->bAllowEmptyOutput = Settings->bAllowEmptyOutputs;
			if (!FMath::IsWithin(PointIO->GetNum(), Min, Max))
			{
				PointIO->OutputPin = PCGExDiscardByPointCount::OutputDiscardedLabel;
			}
		}

		Context->MainPoints->StageOutputs();
		Context->Done();
	}

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
