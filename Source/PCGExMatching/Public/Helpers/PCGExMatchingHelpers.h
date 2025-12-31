// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"

namespace PCGExData
{
	class FFacade;
}

namespace PCGExMatching
{
	class FDataMatcher;
}

struct FPCGPinProperties;
struct FPCGExMatchingDetails;

namespace PCGExMatching::Helpers
{
	PCGEXMATCHING_API
	void DeclareMatchingRulesInputs(const FPCGExMatchingDetails& InDetails, TArray<FPCGPinProperties>& PinProperties);

	PCGEXMATCHING_API
	void DeclareMatchingRulesOutputs(const FPCGExMatchingDetails& InDetails, TArray<FPCGPinProperties>& PinProperties);

	PCGEXMATCHING_API
	int32 GetMatchingSourcePartitions(
		TSharedPtr<FDataMatcher>& Matcher,
		const TArray<TSharedPtr<PCGExData::FFacade>>& Facades,
		TArray<TArray<int32>>& OutPartitions,
		bool bExclusive,
		const TSet<int32>* OnceIndices = nullptr);
}
