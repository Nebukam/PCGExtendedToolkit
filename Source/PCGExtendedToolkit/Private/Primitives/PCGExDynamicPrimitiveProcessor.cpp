// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Primitives/PCGExDynamicPrimitiveProcessor.h"

#define LOCTEXT_NAMESPACE "PCGExDynamicPrimitiveProcessorElement"

UPCGExDynamicPrimitiveProcessorSettings::UPCGExDynamicPrimitiveProcessorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExPointIO::EInit UPCGExDynamicPrimitiveProcessorSettings::GetPointOutputInitMode() const { return PCGExPointIO::EInit::NoOutput; }

TArray<FPCGPinProperties> UPCGExDynamicPrimitiveProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Empty;
	return Empty;
}

FPCGContext* FPCGExDynamicPrimitiveProcessorElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExDynamicPrimitiveProcessorContext* Context = new FPCGExDynamicPrimitiveProcessorContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	const UPCGExDynamicPrimitiveProcessorSettings* Settings = Context->GetInputSettings<UPCGExDynamicPrimitiveProcessorSettings>();
	check(Settings);

	return Context;
}

#undef LOCTEXT_NAMESPACE
