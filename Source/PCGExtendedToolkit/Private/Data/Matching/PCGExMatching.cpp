// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/Matching/PCGExMatching.h"

#include "PCGExCompare.h"

void PCGExMatching::DeclareMatchingRulesInputs(const FPCGExMatchingDetails& InDetails, TArray<FPCGPinProperties>& PinProperties)
{
	if (InDetails.Mode == EPCGExMapMatchMode::Disabled) { return; }

	FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(SourceMatchRulesLabel, EPCGDataType::Param);
	Pin.Tooltip = FTEXT("Matching rules to determine which target data can be paired with each input. If target only accept a single data, individual target points will be evaluated.");
	Pin.PinStatus = InDetails.Mode != EPCGExMapMatchMode::Disabled ? EPCGPinStatus::Required : EPCGPinStatus::Advanced;

	if (InDetails.bClusterMatching && InDetails.ClusterMatchMode == EPCGExClusterComponentTagMatchMode::Separated)
	{
		FPCGPinProperties& EdgePin = PinProperties.Emplace_GetRef(SourceMatchRulesEdgesLabel, EPCGDataType::Param);
		EdgePin.Tooltip = FTEXT("Extra matching rules to determine which edges data can be paired with each input. If target only accept a single data, individual target points will be evaluated.");
		Pin.PinStatus = InDetails.Mode != EPCGExMapMatchMode::Disabled ? EPCGPinStatus::Required : EPCGPinStatus::Advanced;
	}
}

void PCGExMatching::DeclareMatchingRulesOutputs(const FPCGExMatchingDetails& InDetails, TArray<FPCGPinProperties>& PinProperties)
{
	FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(OutputUnmatchedLabel, EPCGDataType::Point);
	Pin.Tooltip = FTEXT("Data that couldn't be matched to any target, and couldn't be processed.");
	Pin.PinStatus = !InDetails.WantsUnmatchedSplit() ? EPCGPinStatus::Advanced : EPCGPinStatus::Normal;
}
