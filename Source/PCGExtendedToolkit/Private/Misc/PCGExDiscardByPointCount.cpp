// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExDiscardByPointCount.h"

#include "PCGExPointsProcessor.h"

#define LOCTEXT_NAMESPACE "PCGExDiscardByPointCountElement"

PCGExPointIO::EInit UPCGExDiscardByPointCountSettings::GetPointOutputInitMode() const { return PCGExPointIO::EInit::NoOutput; }

FPCGElementPtr UPCGExDiscardByPointCountSettings::CreateElement() const { return MakeShared<FPCGExDiscardByPointCountElement>(); }

bool FPCGExDiscardByPointCountElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDiscardByPointCountElement::Execute);

	FPCGExPointsProcessorContext* Context = static_cast<FPCGExPointsProcessorContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
	}

	const UPCGExDiscardByPointCountSettings* Settings = InContext->GetInputSettings<UPCGExDiscardByPointCountSettings>();
	check(Settings);

	auto ProcessInput = [&](FPCGExPointIO& PointIO, int32)
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
