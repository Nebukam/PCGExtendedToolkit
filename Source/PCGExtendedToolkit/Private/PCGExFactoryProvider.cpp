// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExFactoryProvider.h"

#include "PCGExContext.h"

#define LOCTEXT_NAMESPACE "PCGExFactoryProvider"
#define PCGEX_NAMESPACE PCGExFactoryProvider

PCGExFactories::EType UPCGExParamFactoryBase::GetFactoryType() const
{
	return PCGExFactories::EType::Default;
}

TArray<FPCGPinProperties> UPCGExFactoryProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExFactoryProviderSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_PARAM(GetMainOutputLabel(), GetMainOutputLabel().ToString(), Required, {})
	return PinProperties;
}

FPCGElementPtr UPCGExFactoryProviderSettings::CreateElement() const
{
	return MakeShared<FPCGExFactoryProviderElement>();
}

FName UPCGExFactoryProviderSettings::GetMainOutputLabel() const { return TEXT(""); }

#if WITH_EDITOR
FString UPCGExFactoryProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

UPCGExParamFactoryBase* UPCGExFactoryProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	return InFactory;
}

bool FPCGExFactoryProviderElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFactoryProviderElement::Execute);

	PCGEX_SETTINGS(FactoryProvider)

	UPCGExParamFactoryBase* OutFactory = Settings->CreateFactory(Context, nullptr);

	FPCGExContext* PCGExContext = static_cast<FPCGExContext*>(Context);
	check(PCGExContext);

	PCGExContext->FutureOutput(Settings->GetMainOutputLabel(), OutFactory);
	PCGExContext->ExecuteEnd();
	
	return true;
}

FPCGContext* FPCGExFactoryProviderElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExContext* Context = new FPCGExContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;
	return Context;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
