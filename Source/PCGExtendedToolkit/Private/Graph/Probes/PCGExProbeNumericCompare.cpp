// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Probes/PCGExProbeNumericCompare.h"

#include "Graph/Probes/PCGExProbing.h"

PCGEX_CREATE_PROBE_FACTORY(NumericCompare, {}, {})

bool UPCGExProbeNumericCompare::PrepareForPoints(const TSharedPtr<PCGExData::FPointIO>& InPointIO)
{
	if (!Super::PrepareForPoints(InPointIO)) { return false; }

	ValuesBuffer = PrimaryDataFacade->GetScopedBroadcaster<double>(Config.Attribute);

	if (!ValuesBuffer)
	{
		PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FText::FromString(TEXT("Invalid comparison attribute: \"{0}\"")), FText::FromName(Config.Attribute.GetName())));
		return false;
	}


	CWCoincidenceTolerance = FVector(1 / Config.CoincidencePreventionTolerance);

	return true;
}

void UPCGExProbeNumericCompare::ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges)
{
	bool bIsAlreadyConnected;
	const double R = GetSearchRadius(Index);

	TSet<FInt32Vector> LocalCoincidence;

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

		if (PCGExCompare::Compare(Config.Comparison, ValuesBuffer->Read(Index), ValuesBuffer->Read(C.PointIndex), Config.Tolerance))
		{
			OutEdges->Add(PCGEx::H64U(Index, C.PointIndex));
		}
	}
}

void UPCGExProbeNumericCompare::ProcessNode(const int32 Index, const FPCGPoint& Point, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, const TArray<int8>& AcceptConnections)
{
	Super::ProcessNode(Index, Point, nullptr, FVector::ZeroVector, OutEdges, AcceptConnections);
}

#if WITH_EDITOR
FString UPCGExProbeNumericCompareProviderSettings::GetDisplayName() const
{
	return TEXT("");
	/*
	return GetDefaultNodeName().ToString()
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.WeightFactor) / 1000.0));
		*/
}
#endif
