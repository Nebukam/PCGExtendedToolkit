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
		MaxConnectionsCache = PrimaryDataCache->GetOrCreateGetter<int32>(Descriptor.MaxConnectionsAttribute);

		if (!MaxConnectionsCache)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FText::FromString(TEXT("Invalid Max Connections attribute: {0}")), FText::FromName(Descriptor.MaxConnectionsAttribute.GetName())));
			return false;
		}
	}

	CWStackingTolerance = FVector(1 / Descriptor.StackingPreventionTolerance);

	return true;
}

void UPCGExProbeClosest::ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Stacks, const FVector& ST)
{
	bool bIsStacking;
	const int32 MaxIterations = FMath::Min(MaxConnectionsCache ? MaxConnectionsCache->Values[Index] : MaxConnections, Candidates.Num());
	const double R = SearchRadiusCache ? SearchRadiusCache->Values[Index] : SearchRadiusSquared;

	if (MaxIterations <= 0) { return; }

	TSet<uint64> Occupied;
	int32 Additions = 0;

	for (PCGExProbing::FCandidate& C : Candidates)
	{
		if (C.Distance > R) { return; } // Candidates are sorted, stop there.

		if (Stacks)
		{
			Stacks->Add(C.GH, &bIsStacking);
			if (bIsStacking) { continue; }
		}

		if (Descriptor.bPreventStacking)
		{
			Occupied.Add(PCGEx::GH(C.Direction, CWStackingTolerance), &bIsStacking);
			if (bIsStacking) { continue; }
		}

		AddEdge(PCGEx::H64(Index, C.PointIndex));

		Additions++;
		if (Additions >= MaxIterations) { return; }
	}
}

void UPCGExProbeClosest::ProcessNode(const int32 Index, const FPCGPoint& Point, TSet<uint64>* Stacks, const FVector& ST)
{
	Super::ProcessNode(Index, Point, nullptr, FVector::ZeroVector);
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
