// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensorFlow.h"

#define LOCTEXT_NAMESPACE "PCGExCreateTensorFlow"
#define PCGEX_NAMESPACE CreateTensorFlow

PCGEX_TENSOR_BOILERPLATE(Flow)

TArray<FPCGPinProperties> UPCGExCreateTensorFlowSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties =  Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourcePointsLabel, "Single point collection whose transform represent the flow", Required, {})
	return PinProperties;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
