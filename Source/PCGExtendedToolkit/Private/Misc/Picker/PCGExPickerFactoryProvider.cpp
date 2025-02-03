// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Pickers/PCGExPickerFactoryProvider.h"
#include "Misc/Pickers/PCGExPickerOperation.h"

#define LOCTEXT_NAMESPACE "PCGExCreatePicker"
#define PCGEX_NAMESPACE CreatePicker

UPCGExPickerOperation* UPCGExPickerFactoryData::CreateOperation(FPCGExContext* InContext) const
{
	return nullptr; // Create shape builder operation
}

bool UPCGExPickerFactoryData::Prepare(FPCGExContext* InContext)
{
	if (!Super::Prepare(InContext)) { return false; }
	return InitInternalData(InContext);
}

bool UPCGExPickerFactoryData::InitInternalData(FPCGExContext* InContext)
{
	return true;
}

TArray<FPCGPinProperties> UPCGExPickerFactoryProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	return PinProperties;
}

UPCGExFactoryData* UPCGExPickerFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	return Super::CreateFactory(InContext, InFactory);
}

bool UPCGExPickerPointFactoryData::InitInternalData(FPCGExContext* InContext)
{
	if (!Super::InitInternalData(InContext)) { return false; }

	//InputDataFacade = PCGExData::TryGetSingleFacade(InContext, PCGExPicker::SourceEffectorsLabel, true);

	return true;
}

bool UPCGExPickerPointFactoryProviderSettings::RequiresInputs() const
{
	return false;
}

TArray<FPCGPinProperties> UPCGExPickerPointFactoryProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (RequiresInputs()) { PCGEX_PIN_POINT(PCGEx::SourceSourcesLabel, "Source collections or attribute set to read values from", Required, {}) }
	return PinProperties;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
