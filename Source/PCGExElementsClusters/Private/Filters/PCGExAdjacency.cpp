// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Filters/PCGExAdjacency.h"

#include "Core/PCGExContext.h"
#include "Data/PCGExData.h"
#include "Clusters/PCGExCluster.h"

bool FPCGExAdjacencySettings::Init(const FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InPrimaryDataFacade, const bool bQuiet)
{
	bUseDiscreteMeasure = ThresholdType == EPCGExMeanMeasure::Discrete;
	bUseLocalThreshold = ThresholdInput == EPCGExInputValueType::Attribute;
	bTestAllNeighbors = Mode != EPCGExAdjacencyTestMode::Some;

	if (bUseLocalThreshold)
	{
		LocalThreshold = InPrimaryDataFacade->GetBroadcaster<double>(ThresholdAttribute);
		if (!LocalThreshold)
		{
			if (!bQuiet) { PCGEX_LOG_INVALID_SELECTOR_C(InContext, Local Threshold, ThresholdAttribute) }
			return false;
		}
	}

	return true;
}

int32 FPCGExAdjacencySettings::GetThreshold(const PCGExClusters::FNode& Node) const
{
	auto InternalEnsure = [&](const int32 Value)-> int32
	{
		switch (ThresholdComparison)
		{
		case EPCGExComparison::StrictlyEqual: return Node.Num() < Value ? -1 : Value;
		case EPCGExComparison::StrictlyNotEqual: return Value;
		case EPCGExComparison::EqualOrGreater: return Node.Num() < Value ? -1 : Value;
		case EPCGExComparison::EqualOrSmaller: return Value;
		case EPCGExComparison::StrictlyGreater: return Node.Num() <= Value ? -1 : Value;
		case EPCGExComparison::StrictlySmaller: return Value;
		case EPCGExComparison::NearlyEqual: return Value;
		case EPCGExComparison::NearlyNotEqual: return Value;
		default: return Value;
		}
	};

	if (bUseLocalThreshold)
	{
		if (bUseDiscreteMeasure)
		{
			// Fetch absolute subset count from node
			return InternalEnsure(LocalThreshold->Read(Node.PointIndex));
		}

		// Fetch relative subset count from node and factor the local adjacency count
		switch (Rounding)
		{
		default: ;
		case EPCGExRelativeThresholdRoundingMode::Round: return FMath::RoundToInt32(LocalThreshold->Read(Node.PointIndex) * Node.Num());
		case EPCGExRelativeThresholdRoundingMode::Floor: return FMath::FloorToInt32(LocalThreshold->Read(Node.PointIndex) * Node.Num());
		case EPCGExRelativeThresholdRoundingMode::Ceil: return FMath::CeilToInt32(LocalThreshold->Read(Node.PointIndex) * Node.Num());
		}
	}

	if (bUseDiscreteMeasure)
	{
		// Use constant measure from settings
		return InternalEnsure(DiscreteThreshold);
	}

	switch (Rounding)
	{
	default: ;
	case EPCGExRelativeThresholdRoundingMode::Round: return FMath::RoundToInt32(RelativeThreshold * Node.Num());
	case EPCGExRelativeThresholdRoundingMode::Floor: return FMath::FloorToInt32(RelativeThreshold * Node.Num());
	case EPCGExRelativeThresholdRoundingMode::Ceil: return FMath::CeilToInt32(RelativeThreshold * Node.Num());
	}
}
