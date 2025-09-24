// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/Matching/PCGExMatching.h"

#include "PCGExCompare.h"
#include "Data/Matching/PCGExMatchRuleFactoryProvider.h"

void PCGExMatching::DeclareMatchingRulesInputs(const FPCGExMatchingDetails& InDetails, TArray<FPCGPinProperties>& PinProperties)
{
	if (InDetails.Mode == EPCGExMapMatchMode::Disabled) { return; }

	{
		FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(SourceMatchRulesLabel, FPCGExMatchRuleDataTypeInfo::AsId());
		PCGEX_PIN_TOOLTIP("Matching rules to determine which target data can be paired with each input. If target only accept a single data, individual target points will be evaluated.")
		Pin.PinStatus = InDetails.Mode != EPCGExMapMatchMode::Disabled ? EPCGPinStatus::Required : EPCGPinStatus::Advanced;
	}

	if (InDetails.Usage == EPCGExMatchingDetailsUsage::Cluster && InDetails.ClusterMatchMode == EPCGExClusterComponentTagMatchMode::Separated)
	{
		FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(SourceMatchRulesEdgesLabel, FPCGExMatchRuleDataTypeInfo::AsId());
		PCGEX_PIN_TOOLTIP("Extra matching rules to determine which edges data can be paired with each input. If target only accept a single data, individual target points will be evaluated.")
		Pin.PinStatus = InDetails.Mode != EPCGExMapMatchMode::Disabled ? EPCGPinStatus::Required : EPCGPinStatus::Advanced;
	}
}

void PCGExMatching::DeclareMatchingRulesOutputs(const FPCGExMatchingDetails& InDetails, TArray<FPCGPinProperties>& PinProperties)
{
	FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(OutputUnmatchedLabel, EPCGDataType::Point);
	PCGEX_PIN_TOOLTIP("Data that couldn't be matched to any target, and couldn't be processed.")
	Pin.PinStatus = !InDetails.WantsUnmatchedSplit() ? EPCGPinStatus::Advanced : EPCGPinStatus::Normal;
}
