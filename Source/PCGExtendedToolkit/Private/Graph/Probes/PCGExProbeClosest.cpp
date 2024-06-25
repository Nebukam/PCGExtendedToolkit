// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Probes/PCGExProbeClosest.h"

#include "Graph/Probes/PCGExProbing.h"

PCGEX_CREATE_PROBE_FACTORY(Closest, {}, {})

bool UPCGExProbeClosest::RequiresDirectProcessing()
{
	return Descriptor.bUnbounded;
}

bool UPCGExProbeClosest::PrepareForPoints(const PCGExData::FPointIO* InPointIO)
{
	if (!Super::PrepareForPoints(InPointIO)) { return false; }

	if (Descriptor.MaxConnectionsSource == EPCGExFetchType::Constant)
	{
		MaxConnections = Descriptor.MaxConnectionsConstant;
	}
	else
	{
		PCGEx::FLocalIntegerGetter* NumGetter = new PCGEx::FLocalIntegerGetter();
		NumGetter->Capture(Descriptor.MaxConnectionsAttribute);

		if (!NumGetter->Grab(InPointIO))
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FText::FromString(TEXT("Invalid Max Connections attribute: {0}")), FText::FromName(Descriptor.MaxConnectionsAttribute.GetName())));
			PCGEX_DELETE(NumGetter)
			return false;
		}

		MaxConnectionsCache.Reserve(InPointIO->GetNum());
		MaxConnectionsCache.Append(NumGetter->Values);
		PCGEX_DELETE(NumGetter)
	}

	CWStackingTolerance = FVector(Descriptor.StackingDetectionTolerance);

	return true;
}

void UPCGExProbeClosest::ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates)
{
	const int32 MaxIterations = FMath::Min(MaxConnections == -1 ? MaxConnectionsCache[Index] : MaxConnections, Candidates.Num());
	const double R = SearchRadiusSquared == -1 ? SearchRadiusCache[Index] : SearchRadiusSquared;

	if (MaxIterations <= 0) { return; }

	TSet<uint64> Occupied;
	bool bIsOccupied;
	int32 Additions = 0;

	for (PCGExProbing::FCandidate& C : Candidates)
	{
		if (C.Distance > R) { return; } // Candidates are sorted, stop there.

		if (Descriptor.bPreventStacking)
		{
			Occupied.Add(PCGEx::GH(C.Direction, CWStackingTolerance), &bIsOccupied);
			if (bIsOccupied) { continue; }
		}

		AddEdge(PCGEx::H64(Index, C.Index));

		Additions++;
		if (Additions >= MaxIterations) { return; }
	}
}

void UPCGExProbeClosest::ProcessNode(const int32 Index, const FPCGPoint& Point)
{
	Super::ProcessNode(Index, Point);
}

#if WITH_EDITOR
FString UPCGExProbeClosestProviderSettings::GetDisplayName() const
{
	return TEXT("");
	/*
	return GetDefaultNodeName().ToString()
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Descriptor.WeightFactor) / 1000.0));
		*/
}
#endif
