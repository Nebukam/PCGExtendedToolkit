// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExClipper2Inflate.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExSettingsDetails.h"

#define LOCTEXT_NAMESPACE "PCGExClipper2InflateElement"
#define PCGEX_NAMESPACE Clipper2Inflate

PCGEX_INITIALIZE_ELEMENT(Clipper2Inflate)

FPCGExGeo2DProjectionDetails UPCGExClipper2InflateSettings::GetProjectionDetails() const
{
	return ProjectionDetails;
}

void FPCGExClipper2InflateContext::Process(const TArray<int32>& Subjects, const TArray<int32>* Operands)
{
	// TODO : Implement
	// Note : we may want to optionally do a union of all incoming paths; with our without fill (if processing paths only not as a polygons)
	for (int32 SIdx : Subjects)
	{
		auto OffsetValue = OffsetValues[SIdx];
		// OffsetValue->Read(PointIndex); -> value offset of the point at index PointIndex originating from Main SIdx
	}
}

bool FPCGExClipper2InflateElement::PostBoot(FPCGExContext* InContext) const
{
	PCGEX_CONTEXT_AND_SETTINGS(Clipper2Inflate)

	Context->OffsetValues.Reserve(Context->AllOpData->Num());

	for (const TSharedPtr<PCGExData::FFacade>& Facade : *Context->AllOpData->Facades.Get())
	{
		auto OffsetSetting = Settings->Offset.GetValueSetting();
		if (!OffsetSetting->Init(Facade)) { return false; }
		Context->OffsetValues.Add(OffsetSetting);
	}

	return FPCGExClipper2ProcessorElement::PostBoot(InContext);
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
