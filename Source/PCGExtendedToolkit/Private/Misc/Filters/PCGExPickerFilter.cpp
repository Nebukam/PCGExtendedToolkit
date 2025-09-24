// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExPickerFilter.h"


#include "Misc/Pickers/PCGExPicker.h"
#include "Transform/Tensors/PCGExTensorFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExPickerFilterDefinition"
#define PCGEX_NAMESPACE PCGExPickerFilterDefinition

bool UPCGExPickerFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }

	if (!PCGExFactories::GetInputFactories(InContext, PCGExPicker::SourcePickersLabel, PickerFactories, {PCGExFactories::EType::IndexPicker}, true)) { return false; }
	if (PickerFactories.IsEmpty())
	{
		if (!bQuietMissingInputError) { PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Missing pickers.")); }
		return false;
	}

	return true;
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExPickerFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FPickerFilter>(this);
}

bool PCGExPointFilter::FPickerFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }

	for (const TObjectPtr<const UPCGExPickerFactoryData>& FactoryData : TypedFilterFactory->PickerFactories)
	{
		FactoryData->AddPicks(InPointDataFacade->GetNum(), Picks);
	}

	return true;
}

bool PCGExPointFilter::FPickerFilter::Test(const int32 PointIndex) const
{
	return TypedFilterFactory->Config.bInvert ? !Picks.Contains(PointIndex) : Picks.Contains(PointIndex);
}

bool PCGExPointFilter::FPickerFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
{
	if (!ParentCollection) { return false; }

	const int32 NumEntries = ParentCollection->Num();
	for (const TObjectPtr<const UPCGExPickerFactoryData>& FactoryData : TypedFilterFactory->PickerFactories)
	{
		TSet<int32> TempPicks;
		FactoryData->AddPicks(NumEntries, TempPicks);
		if (TempPicks.Contains(IO->IOIndex)) { return !TypedFilterFactory->Config.bInvert; }
	}

	return TypedFilterFactory->Config.bInvert;
}

TArray<FPCGPinProperties> UPCGExPickerFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExPicker::SourcePickersLabel, "Pickers", Required, FPCGExPickerDataTypeInfo)
	return PinProperties;
}

PCGEX_CREATE_FILTER_FACTORY(Picker)

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
