// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExClipper2Offset.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExSettingsDetails.h"

#define LOCTEXT_NAMESPACE "PCGExClipper2OffsetElement"
#define PCGEX_NAMESPACE Clipper2Offset

PCGEX_INITIALIZE_ELEMENT(Clipper2Offset)

FPCGExGeo2DProjectionDetails UPCGExClipper2OffsetSettings::GetProjectionDetails() const
{
	return ProjectionDetails;
}

void FPCGExClipper2OffsetContext::Process(const TArray<int32>& Subjects, const TArray<int32>* Operands)
{
	// TODO : Implement
	// NOTE : We may want to offer the possibility to a union of the group before doing an offset.
	// This is only relevant is matching is enabled
	for (int32 SIdx : Subjects)
	{
		auto OffsetValue = OffsetValues[SIdx];
		// OffsetValue->Read(PointIndex); -> value offset of the point at index PointIndex originating from Main SIdx
	}
}

bool FPCGExClipper2OffsetElement::PostBoot(FPCGExContext* InContext) const
{
	PCGEX_CONTEXT_AND_SETTINGS(Clipper2Offset)

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
