// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Probes/PCGExProbeAnisotropic.h"

#include "Graph/Probes/PCGExProbing.h"

PCGEX_CREATE_PROBE_FACTORY(Anisotropic, {}, {})

bool UPCGExProbeAnisotropic::PrepareForPoints(const TSharedPtr<PCGExData::FPointIO>& InPointIO)
{
	if (!Super::PrepareForPoints(InPointIO)) { return false; }

	return true;
}

void UPCGExProbeAnisotropic::ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges)
{
	bool bIsAlreadyConnected;
	const double R = SearchRadiusCache ? SearchRadiusCache->Read(Index) : SearchRadiusSquared;
	const FVector* SampleDirections = Config.CardinalDirectionsOnly ? CardinalDirections : Directions;
	const int32 NumDirections = Config.CardinalDirectionsOnly ? 4 : 16;

	TArray<FVector> D;
	D.Init(FVector(0), NumDirections);

	TArray<int32> BestCandidate;
	BestCandidate.Init(-1, NumDirections);

	TArray<double> BestDot;
	BestDot.Init( 0.95, NumDirections);

	for (int d = 0; d < NumDirections; d++)
	{
		if (Config.bTransformDirection)
		{
			D[d] = SampleDirections[d];
		}

		else
		{
			D[d] = Point.Transform.TransformVectorNoScale(SampleDirections[d]);
		}
	}

	for (int i = 0; i < Candidates.Num(); i++)
	{
		const PCGExProbing::FCandidate& C = Candidates[i];

		if (C.Distance > R) { break; }
		if (Coincidence && Coincidence->Contains(C.GH)) { continue; }

		int32 BestIndex = -1;

		for (int d = 0; d < NumDirections; d++)
		{
			const double TempDot = FVector::DotProduct(D[d], C.Direction);
			if (TempDot > BestDot[d])
			{
				BestIndex = d;
				BestDot[d] = TempDot;
			}
		}

		if (BestIndex != -1)
		{
			if (Coincidence)
			{
				Coincidence->Add(C.GH, &bIsAlreadyConnected);
				if (bIsAlreadyConnected) { continue; }
			}

			BestCandidate[BestIndex] = i;
		}
	}

	for (int d = 0; d < NumDirections; d++)
	{
		if (BestCandidate[d] == -1) { continue; }
		const PCGExProbing::FCandidate& C = Candidates[BestCandidate[d]];
		OutEdges->Add(PCGEx::H64U(Index, C.PointIndex));
	}
}

#if WITH_EDITOR
FString UPCGExProbeAnisotropicProviderSettings::GetDisplayName() const
{
	return TEXT("");
	/*
	return GetDefaultNodeName().ToString()
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.WeightFactor) / 1000.0));
		*/
}
#endif
