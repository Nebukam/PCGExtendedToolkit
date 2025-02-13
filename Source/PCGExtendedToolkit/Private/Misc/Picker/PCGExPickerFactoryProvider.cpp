// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Pickers/PCGExPickerFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExCreatePicker"
#define PCGEX_NAMESPACE CreatePicker

void UPCGExPickerFactoryData::AddPicks(const int32 InNum, TSet<int32>& OutPicks) const
{
}

bool UPCGExPickerFactoryData::Prepare(FPCGExContext* InContext)
{
	if (!Super::Prepare(InContext)) { return false; }
	return InitInternalData(InContext);
}

bool UPCGExPickerFactoryData::RequiresInputs() const
{
	return false;
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

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
