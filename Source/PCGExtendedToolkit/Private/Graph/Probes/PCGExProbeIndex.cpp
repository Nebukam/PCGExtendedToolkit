// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Probes/PCGExProbeIndex.h"
#include "PCGExMath.h"

PCGEX_CREATE_PROBE_FACTORY(Index, {}, {})

bool UPCGExProbeIndex::RequiresDirectProcessing()
{
	return true;
}

bool UPCGExProbeIndex::PrepareForPoints(const TSharedPtr<PCGExData::FPointIO>& InPointIO)
{
	if (!Super::PrepareForPoints(InPointIO)) { return false; }

	MaxIndex = PointIO->GetNum() - 1;

#define PCGEX_FOREACH_SANITIZEINDEX(MACRO, _VALUE)\
	switch (Config.IndexSafety) {\
	default: case EPCGExIndexSafety::Ignore:	MACRO(EPCGExIndexSafety::Ignore, _VALUE)	break;\
	case EPCGExIndexSafety::Tile:		MACRO(EPCGExIndexSafety::Tile, _VALUE) break;\
	case EPCGExIndexSafety::Clamp:		MACRO(EPCGExIndexSafety::Clamp, _VALUE) break;\
	case EPCGExIndexSafety::Yoyo:		MACRO(EPCGExIndexSafety::Yoyo, _VALUE) break;	}

#define PCGEX_TARGET_CONNECT_TARGET(_MODE, _VALUE)\
	TryCreateEdge = [&](const int32 Index, TSet<uint64>* OutEdges) {\
	const int32 Value = PCGExMath::SanitizeIndex<int32, _MODE>(_VALUE, MaxIndex);\
	if (Value != -1) { OutEdges->Add(PCGEx::H64U(Index, Value)); }};

#define PCGEX_TARGET_CONNECT_ONEWAY(_MODE, _VALUE)\
	TryCreateEdge = [&](const int32 Index, TSet<uint64>* OutEdges) {\
	const int32 Value = PCGExMath::SanitizeIndex<int32, _MODE>(Index + _VALUE, MaxIndex);\
	if (Value != -1) { OutEdges->Add(PCGEx::H64U(Index, Value)); }};

#define PCGEX_TARGET_CONNECT_TWOWAY(_MODE, _VALUE)\
	TryCreateEdge = [&](const int32 Index, TSet<uint64>* OutEdges) {\
	const int32 A = PCGExMath::SanitizeIndex<int32, _MODE>(Index + _VALUE, MaxIndex);\
	if (A != -1) { OutEdges->Add(PCGEx::H64U(Index, A)); }\
	const int32 B = PCGExMath::SanitizeIndex<int32, _MODE>(Index - _VALUE, MaxIndex);\
	if (B != -1) { OutEdges->Add(PCGEx::H64U(Index, B)); } };

#define PCGEX_TARGET_CONNECT_SWITCH(_VALUE)\
	switch (Config.Mode){\
	default: case EPCGExProbeTargetMode::Target: PCGEX_FOREACH_SANITIZEINDEX(PCGEX_TARGET_CONNECT_TARGET, _VALUE) break;\
	case EPCGExProbeTargetMode::OneWayOffset: PCGEX_FOREACH_SANITIZEINDEX(PCGEX_TARGET_CONNECT_ONEWAY, _VALUE) break;\
	case EPCGExProbeTargetMode::TwoWayOffset: PCGEX_FOREACH_SANITIZEINDEX(PCGEX_TARGET_CONNECT_TWOWAY, _VALUE) break; }

	if (Config.TargetIndex == EPCGExFetchType::Attribute)
	{
		TargetCache = PrimaryDataFacade->GetScopedBroadcaster<int32>(Config.TargetAttribute);
		if (!TargetCache)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FText::FromString(TEXT("Invalid Target attribute: \"{0}\"")), FText::FromName(Config.TargetAttribute.GetName())));
			return false;
		}

		PCGEX_TARGET_CONNECT_SWITCH(TargetCache->Read(Index))
	}
	else
	{
		PCGEX_TARGET_CONNECT_SWITCH(Config.TargetConstant)
	}

#undef PCGEX_FOREACH_SANITIZEINDEX
#undef PCGEX_TARGET_CONNECT_TARGET
#undef PCGEX_TARGET_CONNECT_ONEWAY
#undef PCGEX_TARGET_CONNECT_TWOWAY
#undef PCGEX_TARGET_CONNECT_SWITCH

	return true;
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
