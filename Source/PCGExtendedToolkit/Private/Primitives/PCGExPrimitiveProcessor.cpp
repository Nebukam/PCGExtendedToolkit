// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Primitives/PCGExPrimitiveProcessor.h"

#define LOCTEXT_NAMESPACE "PCGExPrimitiveProcessorElement"

UPCGExPrimitiveProcessorSettings::UPCGExPrimitiveProcessorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

PCGExPointIO::EInit UPCGExPrimitiveProcessorSettings::GetPointOutputInitMode() const { return PCGExPointIO::EInit::NoOutput; }

TArray<FPCGPinProperties> UPCGExPrimitiveProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Empty;
	return Empty;
}

FPCGContext* FPCGExPrimitiveProcessorElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExPrimitiveProcessorContext* Context = new FPCGExPrimitiveProcessorContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	const UPCGExPrimitiveProcessorSettings* Settings = Context->GetInputSettings<UPCGExPrimitiveProcessorSettings>();
	check(Settings);

	return Context;
}

#undef LOCTEXT_NAMESPACE
