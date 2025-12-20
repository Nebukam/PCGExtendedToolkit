// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Matching/PCGExMatchingLabels.h"

#include "Matching/PCGExMatchRuleFactoryProvider.h"

void PCGExMatching::DeclareMatchingRulesInputs(const FPCGExMatchingDetails& InDetails, TArray<FPCGPinProperties>& PinProperties)
{
	{
		FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(Labels::SourceMatchRulesLabel, FPCGExDataTypeInfoMatchRule::AsId());
		PCGEX_PIN_TOOLTIP("Matching rules to determine which target data can be paired with each input. If target only accept a single data, individual target points will be evaluated.")
		Pin.PinStatus = InDetails.Mode != EPCGExMapMatchMode::Disabled ? EPCGPinStatus::Required : EPCGPinStatus::Advanced;
	}

	if (InDetails.Usage == EPCGExMatchingDetailsUsage::Cluster && InDetails.ClusterMatchMode == EPCGExClusterComponentTagMatchMode::Separated)
	{
		FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(Labels::SourceMatchRulesEdgesLabel, FPCGExDataTypeInfoMatchRule::AsId());
		PCGEX_PIN_TOOLTIP("Extra matching rules to determine which edges data can be paired with each input. If target only accept a single data, individual target points will be evaluated.")
		Pin.PinStatus = InDetails.Mode != EPCGExMapMatchMode::Disabled ? EPCGPinStatus::Required : EPCGPinStatus::Advanced;
	}
}

void PCGExMatching::DeclareMatchingRulesOutputs(const FPCGExMatchingDetails& InDetails, TArray<FPCGPinProperties>& PinProperties)
{
	const bool bShowAsAdvanced = !InDetails.WantsUnmatchedSplit();
	if (InDetails.Usage == EPCGExMatchingDetailsUsage::Cluster)
	{
		{
			FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(Labels::OutputUnmatchedVtxLabel, EPCGDataType::Point);
			PCGEX_PIN_TOOLTIP("Vtx data that couldn't be matched to any target, and couldn't be processed. Note that Vtx data may exist in regular pin as well, this is to ensure unmatched edges are still bound to valid vtx.")
			Pin.PinStatus = bShowAsAdvanced ? EPCGPinStatus::Advanced : EPCGPinStatus::Normal;
		}
		{
			FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(Labels::OutputUnmatchedEdgesLabel, EPCGDataType::Point);
			PCGEX_PIN_TOOLTIP("Edge data that couldn't be matched to any target, and couldn't be processed.")
			Pin.PinStatus = bShowAsAdvanced ? EPCGPinStatus::Advanced : EPCGPinStatus::Normal;
		}
	}
	else
	{
		FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(Labels::OutputUnmatchedLabel, EPCGDataType::Point);
		PCGEX_PIN_TOOLTIP("Data that couldn't be matched to any target, and couldn't be processed.")
		Pin.PinStatus = bShowAsAdvanced ? EPCGPinStatus::Advanced : EPCGPinStatus::Normal;
	}
}
