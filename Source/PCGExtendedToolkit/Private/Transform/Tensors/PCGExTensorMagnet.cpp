// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensorMagnet.h"

#define LOCTEXT_NAMESPACE "PCGExCreateTensorMagnet"
#define PCGEX_NAMESPACE CreateTensorMagnet

PCGEX_TENSOR_BOILERPLATE(Magnet)

TArray<FPCGPinProperties> UPCGExCreateTensorMagnetSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties =  Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourcePointsLabel, "Single point collection that represent attractors", Required, {})
	return PinProperties;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
