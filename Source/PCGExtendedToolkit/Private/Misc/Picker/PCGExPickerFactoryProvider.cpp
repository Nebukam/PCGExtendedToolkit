// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Pickers/PCGExPickerFactoryProvider.h"



#include "VerseVM/VVMInstantiationContext.h"

#define LOCTEXT_NAMESPACE "PCGExCreatePicker"
#define PCGEX_NAMESPACE CreatePicker

void UPCGExPickerFactoryData::AddPicks(const int32 InNum, TSet<int32>& OutPicks) const
{
}

bool UPCGExPickerFactoryData::Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
{
	if (!Super::Prepare(InContext, AsyncManager)) { return false; }
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

namespace PCGExPicker
{
	bool GetPicks(const TArray<TObjectPtr<const UPCGExPickerFactoryData>>& Factories, const TSharedPtr<PCGExData::FFacade>& InFacade, TSet<int32>& OutPicks)
	{
		if (Factories.IsEmpty()) { return false; }
		
		const int32 Num = InFacade->GetNum();
		for (const TObjectPtr<const UPCGExPickerFactoryData>& Op : Factories) { Op->AddPicks(Num, OutPicks); }
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
