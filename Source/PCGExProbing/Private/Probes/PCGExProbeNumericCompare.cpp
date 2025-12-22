// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Probes/PCGExProbeNumericCompare.h"


#include "Containers/PCGExManagedObjects.h"
#include "Data/PCGExData.h"
#include "Details/PCGExSettingsDetails.h"
#include "Core/PCGExProbingCandidates.h"

PCGEX_SETTING_VALUE_IMPL(FPCGExProbeConfigNumericCompare, MaxConnections, int32, MaxConnectionsInput, MaxConnectionsAttribute, MaxConnectionsConstant)
PCGEX_CREATE_PROBE_FACTORY(NumericCompare, {}, {})

bool FPCGExProbeNumericCompare::PrepareForPoints(FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIO>& InPointIO)
{
	if (!FPCGExProbeOperation::PrepareForPoints(InContext, InPointIO)) { return false; }

	MaxConnections = Config.GetValueSettingMaxConnections();
	if (!MaxConnections->Init(PrimaryDataFacade)) { return false; }

	ValuesBuffer = PrimaryDataFacade->GetBroadcaster<double>(Config.Attribute, true);

	if (!ValuesBuffer)
	{
		PCGEX_LOG_INVALID_SELECTOR_C(Context, Comparison, Config.Attribute)
		return false;
	}


	CWCoincidenceTolerance = FVector(PCGEx::SafeScalarTolerance(Config.CoincidencePreventionTolerance));

	return true;
}

void FPCGExProbeNumericCompare::ProcessCandidates(const int32 Index, const FTransform& WorkingTransform, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, PCGExMT::FScopedContainer* Container)
{
	bool bIsAlreadyConnected;
	const int32 MaxIterations = FMath::Min(MaxConnections->Read(Index), Candidates.Num());
	const double R = GetSearchRadius(Index);

	if (MaxIterations <= 0) { return; }

	TSet<uint64> LocalCoincidence;
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
			LocalCoincidence.Add(PCGEx::SH3(C.Direction, CWCoincidenceTolerance), &bIsAlreadyConnected);
			if (bIsAlreadyConnected) { continue; }
		}

		if (PCGExCompare::Compare(Config.Comparison, ValuesBuffer->Read(Index), ValuesBuffer->Read(C.PointIndex), Config.Tolerance))
		{
			OutEdges->Add(PCGEx::H64U(Index, C.PointIndex));

			Additions++;
			if (Additions >= MaxIterations) { return; }
		}
	}
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
