// Copyright 2025 Timothé Lapetite and contributors
// * 24/02/25 Omer Salomon Fixed - Made the count condition for discarding to be inclusive on MaxPointCount (to fit the comment).
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/Filtering/PCGExDiscardByPointCount.h"


#include "Data/PCGBasePointData.h"
#include "Data/PCGExPointIO.h"

#define LOCTEXT_NAMESPACE "PCGExDiscardByPointCountElement"
#define PCGEX_NAMESPACE DiscardByPointCount

TArray<FPCGPinProperties> UPCGExDiscardByPointCountSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExCommon::Labels::OutputDiscardedLabel, "Discarded outputs.", Normal)
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

bool FPCGExDiscardByPointCountElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
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
				PointIO->OutputPin = PCGExCommon::Labels::OutputDiscardedLabel;
				NumDiscarded++;
			}
		}

		Context->MainPoints->StageOutputs();

		if (NumDiscarded == NumTotal) { Context->OutputData.InactiveOutputPinBitmask |= 1ULL << 0; }
		if (!NumDiscarded) { Context->OutputData.InactiveOutputPinBitmask |= 1ULL << 1; }

		Context->Done();
	}

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
