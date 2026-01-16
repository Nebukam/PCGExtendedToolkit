// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Helpers/PCGExMatchingHelpers.h"
#include "Core/PCGExMatchRuleFactoryProvider.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExMatchingDetails.h"
#include "Helpers/PCGExDataMatcher.h"

namespace PCGExMatching::Helpers
{
	void DeclareMatchingRulesInputs(const FPCGExMatchingDetails& InDetails, TArray<FPCGPinProperties>& PinProperties, const FName InPrimaryLabel)
	{
		{
			FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(InPrimaryLabel.IsNone() ? Labels::SourceMatchRulesLabel : InPrimaryLabel, FPCGExDataTypeInfoMatchRule::AsId());
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

	int32 GetMatchingSourcePartitions(const TSharedPtr<FDataMatcher>& Matcher, const TArray<TSharedPtr<PCGExData::FFacade>>& Facades, TArray<TArray<int32>>& OutPartitions, bool bExclusive, const TSet<int32>* OnceIndices)
	{
		// NOTE : Uses Idx insted of IOIndex
		// This is primarily aimed to help clipper2 module to create sub-groups of paths
		// as well as MergeByTags to deprecate existing API and support non-exclusive groups.
		// Having a way to flag "exclusive" data  (some that can only belong to a single group) would be neat

		const int32 NumSources = Matcher->GetNumSources();
		check(NumSources == Facades.Num())
		OutPartitions.Reserve(NumSources);

		if (bExclusive)
		{
			TSet<int32> DistributedIndices;
			DistributedIndices.Reserve(NumSources);

			for (int i = 0; i < NumSources; ++i)
			{
				if (DistributedIndices.Contains(i)) { continue; }

				TArray<int32>& Partition = OutPartitions.Emplace_GetRef();

				FScope Scope = FScope(NumSources, true);
				Matcher->GetMatchingSourcesIndices(Facades[i]->Source->GetTaggedData(), Scope, Partition, &DistributedIndices);
				Partition.AddUnique(i);

				// Mark all partition members as distributed
				for (const int32 Idx : Partition) { DistributedIndices.Add(Idx); }
			}

			return OutPartitions.Num();
		}

		TSet<int32> DistributedIndices;
		DistributedIndices.Reserve(NumSources);

		for (int i = 0; i < NumSources; ++i)
		{
			TArray<int32>& Partition = OutPartitions.Emplace_GetRef();

			FScope Scope = FScope(NumSources, true);
			Matcher->GetMatchingSourcesIndices(Facades[i]->Source->GetTaggedData(), Scope, Partition);
			Partition.AddUnique(i);
		}

		return OutPartitions.Num();
	}
}
