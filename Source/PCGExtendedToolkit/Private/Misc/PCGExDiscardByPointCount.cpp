// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExDiscardByPointCount.h"

#include "PCGExPointsProcessor.h"

#define LOCTEXT_NAMESPACE "PCGExDiscardByPointCountElement"
#define PCGEX_NAMESPACE DiscardByPointCount

PCGExData::EInit UPCGExDiscardByPointCountSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FPCGElementPtr UPCGExDiscardByPointCountSettings::CreateElement() const { return MakeShared<FPCGExDiscardByPointCountElement>(); }

bool FPCGExDiscardByPointCountElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDiscardByPointCountElement::Execute);

	PCGEX_CONTEXT(PointsProcessor)
	PCGEX_SETTINGS(DiscardByPointCount)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
	}

	auto ProcessInput = [&](const PCGExData::FPointIO& PointIO, int32)
	{
		if (Settings->OutsidePointCountFilter(PointIO.GetNum())) { return; }

		FPCGTaggedData& OutputRef = Context->OutputData.TaggedData.Add_GetRef(PointIO.Source);
		OutputRef.Data = PointIO.GetIn();
		OutputRef.Pin = PointIO.DefaultOutputLabel;
	};

	Context->MainPoints->ForEach(ProcessInput);
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
