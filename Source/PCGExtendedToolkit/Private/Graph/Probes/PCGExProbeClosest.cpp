// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Probes/PCGExProbeClosest.h"

#include "Graph/Probes/PCGExProbing.h"

PCGEX_CREATE_PROBE_FACTORY(Closest, {}, {})

bool UPCGExProbeClosest::RequiresDirectProcessing()
{
	return Config.bUnbounded;
}

bool UPCGExProbeClosest::PrepareForPoints(const TSharedPtr<PCGExData::FPointIO>& InPointIO)
{
	if (!Super::PrepareForPoints(InPointIO)) { return false; }

	if (Config.MaxConnectionsInput == EPCGExInputValueType::Constant)
	{
		MaxConnections = Config.MaxConnectionsConstant;
	}
	else
	{
		MaxConnectionsCache = PrimaryDataFacade->GetScopedBroadcaster<int32>(Config.MaxConnectionsAttribute);

		if (!MaxConnectionsCache)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FText::FromString(TEXT("Invalid Max Connections attribute: \"{0}\"")), FText::FromName(Config.MaxConnectionsAttribute.GetName())));
			return false;
		}
	}

	CWCoincidenceTolerance = FVector(1 / Config.CoincidencePreventionTolerance);

	return true;
}

void UPCGExProbeClosest::ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges)
{
	bool bIsAlreadyConnected;
	const int32 MaxIterations = FMath::Min(MaxConnectionsCache ? MaxConnectionsCache->Read(Index) : MaxConnections, Candidates.Num());
	const double R = SearchRadiusCache ? SearchRadiusCache->Read(Index) : SearchRadiusSquared;

	if (MaxIterations <= 0) { return; }

	TSet<FInt32Vector> LocalCoincidence;
	int32 Additions = 0;

	for (PCGExProbing::FCandidate& C : Candidates)
	{
		if (C.Distance > R) { return; } // Candidates are sorted, stop there.

		if (Coincidence)
		{
			Coincidence->Add(C.GH, &bIsAlreadyConnected);
			if (bIsAlreadyConnected) { continue; }
		}

		if (Config.bPreventCoincidence)
		{
			LocalCoincidence.Add(PCGEx::I323(C.Direction, CWCoincidenceTolerance), &bIsAlreadyConnected);
			if (bIsAlreadyConnected) { continue; }
		}

		//bool bEdgeAlreadyExists;
		OutEdges->Add(PCGEx::H64U(Index, C.PointIndex));
		//if (bEdgeAlreadyExists) { continue; }

		Additions++;
		if (Additions >= MaxIterations) { return; }
	}
}

void UPCGExProbeClosest::ProcessNode(const int32 Index, const FPCGPoint& Point, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges)
{
	Super::ProcessNode(Index, Point, nullptr, FVector::ZeroVector, OutEdges);
}

#if WITH_EDITOR
FString UPCGExProbeClosestProviderSettings::GetDisplayName() const
{
	return TEXT("");
	/*
	return GetDefaultNodeName().ToString()
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.WeightFactor) / 1000.0));
		*/
}
#endif
