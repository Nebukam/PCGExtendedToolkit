// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExFactoryProvider.h"
#include "PCGExContext.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExFactoryProvider"
#define PCGEX_NAMESPACE PCGExFactoryProvider

void UPCGExParamDataBase::OutputConfigToMetadata()
{
}

TArray<FPCGPinProperties> UPCGExFactoryProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExFactoryProviderSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_PARAM(GetMainOutputPin(), GetMainOutputPin().ToString(), Required, {})
	return PinProperties;
}

FPCGElementPtr UPCGExFactoryProviderSettings::CreateElement() const
{
	return MakeShared<FPCGExFactoryProviderElement>();
}

#if WITH_EDITOR
FString UPCGExFactoryProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

UPCGExParamFactoryBase* UPCGExFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	return InFactory;
}

bool FPCGExFactoryProviderElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFactoryProviderElement::Execute);

	PCGEX_SETTINGS(FactoryProvider)

	FPCGExContext* PCGExContext = static_cast<FPCGExContext*>(Context);
	check(PCGExContext);

	UPCGExParamFactoryBase* OutFactory = Settings->CreateFactory(PCGExContext, nullptr);

	if (!OutFactory) { return true; }

	OutFactory->bCleanupConsumableAttributes = Settings->bCleanupConsumableAttributes;
	OutFactory->OutputConfigToMetadata();
	PCGExContext->StageOutput(Settings->GetMainOutputPin(), OutFactory, false);
	PCGExContext->OnComplete();

	return true;
}

FPCGContext* FPCGExFactoryProviderElement::Initialize(const FPCGDataCollection& InputData, const TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExContext* Context = new FPCGExContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;
	return Context;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
