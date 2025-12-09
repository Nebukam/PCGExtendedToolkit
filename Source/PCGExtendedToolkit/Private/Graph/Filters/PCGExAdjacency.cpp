// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/PCGExAdjacency.h"

#include "PCGExContext.h"
#include "PCGExMath.h"
#include "Collections/PCGExBitmaskCollection.h"
#include "Details/PCGExDetailsBitmask.h"
#include "Data/PCGExData.h"
#include "Graph/PCGExCluster.h"

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

int32 FPCGExAdjacencySettings::GetThreshold(const PCGExCluster::FNode& Node) const
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

namespace PCGExAdjacency
{
	void FBitmaskData::Append(const UPCGExBitmaskCollection* InCollection, const double InAngle, EPCGExBitOp Op)
	{
		const int32 Reserve = Directions.Num() + InCollection->NumEntries();

		Bitmasks.Reserve(Reserve);
		Directions.Reserve(Reserve);
		Dots.Reserve(Reserve);

		const double Dot = PCGExMath::DegreesToDot(InAngle);

		for (const FPCGExBitmaskCollectionEntry& Entry : InCollection->Entries)
		{
			FPCGExSimpleBitmask& Bitmask = Bitmasks.Emplace_GetRef();
			Bitmask.Bitmask = Entry.Bitmask.Get();
			Bitmask.Op = Op;
			Directions.Add(Entry.GetDirection());
			Dots.Add(Dot);
		}
	}

	void FBitmaskData::Append(const FPCGExBitmaskRef& InBitmaskRef, const double InAngle)
	{
		FPCGExSimpleBitmask& Bitmask = Bitmasks.Emplace_GetRef();
		FVector& Dir = Directions.Emplace_GetRef();

		if (!InBitmaskRef.TryGetAdjacencyInfos(Dir, Bitmask))
		{
			Bitmasks.Pop();
			Directions.Pop();
			return;
		}

		Dots.Add(PCGExMath::DegreesToDot(InAngle));
	}

	void FBitmaskData::Append(const TArray<FPCGExBitmaskRef>& InBitmaskRef, const double InAngle)
	{
		const int32 Reserve = Directions.Num() + InBitmaskRef.Num();

		Bitmasks.Reserve(Reserve);
		Directions.Reserve(Reserve);
		Dots.Reserve(Reserve);

		for (const FPCGExBitmaskRef& Ref : InBitmaskRef) { Append(Ref, InAngle); }
	}

	void FBitmaskData::MutateMatch(const FVector& InDirection, int64& Flags) const
	{
		const int32 NumEntries = Directions.Num();
		for (int i = 0; i < NumEntries; i++)
		{
			if (FVector::DotProduct(InDirection, Directions[i]) >= Dots[i]) { Bitmasks[i].Mutate(Flags); }
		}
	}

	void FBitmaskData::MutateUnmatch(const FVector& InDirection, int64& Flags) const
	{
		const int32 NumEntries = Directions.Num();
		for (int i = 0; i < NumEntries; i++)
		{
			if (FVector::DotProduct(InDirection, Directions[i]) <= Dots[i]) { Bitmasks[i].Mutate(Flags); }
		}
	}

	TSharedPtr<FBitmaskData> FBitmaskData::Make(const TMap<TObjectPtr<UPCGExBitmaskCollection>, EPCGExBitOp_OR>& InCollections, const TArray<FPCGExBitmaskRef>& InReferences, const double Angle)
	{
		TSharedPtr<FBitmaskData> Data = MakeShared<FBitmaskData>();
		Data = MakeShared<FBitmaskData>();
		Data->Append(InReferences, Angle);
		for (const TPair<TObjectPtr<UPCGExBitmaskCollection>, EPCGExBitOp_OR>& Pair : InCollections)
		{
			if (Pair.Key) { Data->Append(Pair.Key, Angle, PCGExBitmask::GetBitOp(Pair.Value)); }
		}
		return Data;
	}
}
