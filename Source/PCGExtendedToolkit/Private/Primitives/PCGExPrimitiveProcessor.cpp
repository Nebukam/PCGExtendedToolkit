// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Primitives/PCGExPrimitiveProcessor.h"

#define LOCTEXT_NAMESPACE "PCGExPrimitiveProcessorElement"

UPCGExPrimitiveProcessorSettings::UPCGExPrimitiveProcessorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExData::EInit UPCGExPrimitiveProcessorSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

TArray<FPCGPinProperties> UPCGExPrimitiveProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Empty;
	return Empty;
}

PCGEX_INITIALIZE_CONTEXT(PrimitiveProcessor)

#undef LOCTEXT_NAMESPACE
