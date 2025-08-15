// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExSpatialTriage.h"

#include "Data/PCGExDataHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExSpatialTriageElement"
#define PCGEX_NAMESPACE SpatialTriage

TArray<FPCGPinProperties> UPCGExSpatialTriageSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	PCGEX_PIN_ANY(PCGPinConstants::DefaultInputLabel, "Inputs", Required, {})
	PCGEX_PIN_SPATIAL(PCGExSpatialTriage::SourceLabelBounds, "Single spatial data whose bounds will be used to do the triage", Required, {})

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExSpatialTriageSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	PCGEX_PIN_ANY(PCGExSpatialTriage::OutputLabelInside, "Data fully within bounds and relevant", Normal, {})
	PCGEX_PIN_ANY(PCGExSpatialTriage::OutputLabelTouching, "Data intersects bounds but not relevant.", Normal, {})
	PCGEX_PIN_ANY(PCGExSpatialTriage::OutputLabelOutside, "Data neither within nor touching bounds.", Normal, {})

	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(SpatialTriage)

bool FPCGExSpatialTriageElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SpatialTriage)

	return true;
}

bool FPCGExSpatialTriageElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSpatialTriageElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SpatialTriage)
	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		TArray<FPCGTaggedData> TaggedDatas = Context->InputData.GetSpatialInputsByPin(PCGExSpatialTriage::SourceLabelBounds);
		if (TaggedDatas.IsEmpty()) { return Context->CancelExecution(TEXT("No valid bounds.")); }

		const FBox Filter = Cast<UPCGSpatialData>(TaggedDatas[0].Data)->GetBounds();

		TaggedDatas = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

		for (int i = 0; i < TaggedDatas.Num(); ++i)
		{
			FPCGTaggedData& TaggedData = TaggedDatas[i];
			const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(TaggedData.Data);
			FName OutputTo = PCGExSpatialTriage::OutputLabelOutside;
			if (SpatialData)
			{
				const FBox Bounds = SpatialData->GetBounds();
				if (Filter.IsInside(Bounds.GetCenter())) { OutputTo = PCGExSpatialTriage::OutputLabelInside; }
				else if (Filter.Intersect(Bounds)) { OutputTo = PCGExSpatialTriage::OutputLabelTouching; }
			}

			Context->StageOutput(const_cast<UPCGData*>(TaggedData.Data.Get()), OutputTo, TaggedData.Tags, false, false, false);
		}
	}

	Context->Done();

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
