// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Helpers/PCGExMatchingHelpers.h"
#include "Core/PCGExMatchRuleFactoryProvider.h"
#include "Details/PCGExMatchingDetails.h"
#include "Helpers/PCGExDataMatcher.h"

namespace PCGExMatching::Helpers
{
	void DeclareMatchingRulesInputs(const FPCGExMatchingDetails& InDetails, TArray<FPCGPinProperties>& PinProperties)
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

	void DeclareMatchingRulesOutputs(const FPCGExMatchingDetails& InDetails, TArray<FPCGPinProperties>& PinProperties)
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

	void GetMatchingSourcePartitions(TSharedPtr<FDataMatcher>& Matcher, const TArray<TSharedPtr<PCGExData::FFacade>>& Facades, TArray<TArray<int32>>& OutPartitions, bool bExclusive)
	{
		// TODO : Partition facades assuming the matcher contains those same facades.
		// This is primarily aimed to help clipper2 module to create sub-groups of paths
		// as well as MergeByTags to deprecate existing API and support non-exclusive groups.
		// Having a way to flag "exclusive" data  (some that can only belong to a single group) would be neat
		// TODO : Merge matcher API so they support FPCGExTaggedData instead
		/*
		const int32 NumSources = Matcher->GetNumSources();
		OutPartitions.Reserve(NumSources);
		
		if (bExclusive)
		{
			TSet<int32> ExcludedIndices;
			ExcludedIndices.Reserve(NumSources);
		
			for (int i = 0; i < NumSources; ++i)
			{
				Matcher->GetMatchingSourcesIndices();
			}
		}
		*/
	}
}
