// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Probes/PCGExProbeAnisotropic.h"

#include "Graph/Probes/PCGExProbing.h"

PCGEX_CREATE_PROBE_FACTORY(Anisotropic, {}, {})

bool UPCGExProbeAnisotropic::PrepareForPoints(const PCGExData::FPointIO* InPointIO)
{
	if (!Super::PrepareForPoints(InPointIO)) { return false; }

	return true;
}

void UPCGExProbeAnisotropic::ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* ConnectedSet, const FVector& ST, TSet<uint64>* OutEdges)
{
	bool bIsAlreadyConnected;
	const double R = SearchRadiusCache ? SearchRadiusCache->Values[Index] : SearchRadiusSquared;

	FVector D[16];
	int32 BestCandidate[16] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,};
	double BestDot[16];
	double BestDist[16];
	if (Config.bTransformDirection)
	{
		for (int d = 0; d < 16; d++)
		{
			D[d] = Directions[d];
			BestDot[d] = 0.92;
			BestDist[d] = TNumericLimits<double>::Max();
		}
	}
	else
	{
		for (int d = 0; d < 16; d++)
		{
			D[d] = Point.Transform.TransformVectorNoScale(Directions[d]);
			BestDot[d] = 0.92;
			BestDist[d] = TNumericLimits<double>::Max();
		}
	}
	
	for (int i = 0; i < Candidates.Num(); i++)
	{
		const PCGExProbing::FCandidate& C = Candidates[i];

		if (C.Distance > R) { break; }
		if (ConnectedSet && ConnectedSet->Contains(C.GH)) { continue; }

		double BatchBestDot = 0.92;
		double BatchBestDist = TNumericLimits<double>::Max();
		int32 BatchBest = -1;
		
		for (int d = 0; d < 16; d++)
		{
			const double TempDot = FVector::DotProduct(D[d], C.Direction);
			if (TempDot >= BestDot[d]) // 22.5 degree tolerance
			{
				if (C.Distance < BatchBestDist)
				{
					BatchBest = d;
					BatchBestDot = TempDot;
					BatchBestDist = C.Distance;
				}
			}
		}

		if(BatchBest != -1)
		{
			BestDist[BatchBest] = BatchBestDist;
			BestDot[BatchBest] = BatchBestDot;
			BestCandidate[BatchBest] = i;
		}
	}

	for (int d = 0; d < 16; d++)
	{
		if (BestCandidate[d] == -1) { continue; }
		const PCGExProbing::FCandidate& C = Candidates[BestCandidate[d]];

		if (ConnectedSet)
		{
			ConnectedSet->Add(C.GH, &bIsAlreadyConnected);
			if (bIsAlreadyConnected) { continue; }
		}

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
