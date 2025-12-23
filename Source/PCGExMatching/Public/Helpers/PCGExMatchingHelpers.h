// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"

struct FPCGPinProperties;
struct FPCGExMatchingDetails;

namespace PCGExMatching::Helpers
{
	PCGEXMATCHING_API
	void DeclareMatchingRulesInputs(const FPCGExMatchingDetails& InDetails, TArray<FPCGPinProperties>& PinProperties);

	PCGEXMATCHING_API
	void DeclareMatchingRulesOutputs(const FPCGExMatchingDetails& InDetails, TArray<FPCGPinProperties>& PinProperties);
}
