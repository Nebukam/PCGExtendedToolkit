// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"

struct FPCGPinProperties;
struct FPCGExMatchingDetails;

namespace PCGExMatching
{
	namespace Labels
	{
		const FName OutputMatchRuleLabel = TEXT("Match Rule");
		const FName SourceMatchRulesLabel = TEXT("Match Rules");
		const FName SourceMatchRulesEdgesLabel = TEXT("Match Rules (Edges)");
		const FName OutputUnmatchedLabel = TEXT("Unmatched");
		const FName OutputUnmatchedVtxLabel = TEXT("Unmatched Vtx");
		const FName OutputUnmatchedEdgesLabel = TEXT("Unmatched Edges");
	}

	PCGEXFOUNDATIONS_API void DeclareMatchingRulesInputs(const FPCGExMatchingDetails& InDetails, TArray<FPCGPinProperties>& PinProperties);

	PCGEXFOUNDATIONS_API void DeclareMatchingRulesOutputs(const FPCGExMatchingDetails& InDetails, TArray<FPCGPinProperties>& PinProperties);
}
