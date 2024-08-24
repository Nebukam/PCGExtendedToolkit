// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Probes/PCGExProbeIndex.h"

PCGEX_CREATE_PROBE_FACTORY(Index, {}, {})

bool UPCGExProbeIndex::RequiresDirectProcessing()
{
	return true;
}

bool UPCGExProbeIndex::PrepareForPoints(const PCGExData::FPointIO* InPointIO)
{
	if (!Super::PrepareForPoints(InPointIO)) { return false; }

	MaxIndex = PointIO->GetNum() - 1;

	if (Config.TargetIndex == EPCGExFetchType::Attribute)
	{
		TargetCache = PrimaryDataFacade->GetScopedBroadcaster<int32>(Config.TargetAttribute);
		if (!TargetCache)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FText::FromString(TEXT("Invalid Target attribute: \"{0}\"")), FText::FromName(Config.TargetAttribute.GetName())));
			return false;
		}
	}

	return true;
}

void UPCGExProbeIndex::ProcessNode(const int32 Index, const FPCGPoint& Point, TSet<uint64>* Stacks, const FVector& ST, TSet<uint64>* OutEdges)
{
	// TODO : Implement Stacking mngmt
	int32 Value = TargetCache ? TargetCache->Values[Index] : Config.TargetConstant;

	if (Config.Mode == EPCGExProbeTargetMode::Target)
	{
		Value = PCGExMath::SanitizeIndex(Value, MaxIndex, Config.IndexSafety);
		if (Value == -1) { return; }
		OutEdges->Add(PCGEx::H64U(Index, Value));
		return;
	}

	if (Config.Mode == EPCGExProbeTargetMode::OneWayOffset)
	{
		Value = PCGExMath::SanitizeIndex(Index + Value, MaxIndex, Config.IndexSafety);
		if (Value == -1) { return; }
		OutEdges->Add(PCGEx::H64U(Index, Value));
		return;
	}

	if (Config.Mode == EPCGExProbeTargetMode::TwoWayOffset)
	{
		int32 OIdx = PCGExMath::SanitizeIndex(Index + Value, MaxIndex, Config.IndexSafety);
		if (OIdx != -1) { OutEdges->Add(PCGEx::H64U(Index, OIdx)); }

		OIdx = PCGExMath::SanitizeIndex(Index - Value, MaxIndex, Config.IndexSafety);
		if (OIdx != -1) { OutEdges->Add(PCGEx::H64U(Index, OIdx)); }
	}
}

#if WITH_EDITOR
FString UPCGExProbeIndexProviderSettings::GetDisplayName() const
{
	return TEXT("");
	/*
	return GetDefaultNodeName().ToString()
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.WeightFactor) / 1000.0));
		*/
}
#endif
