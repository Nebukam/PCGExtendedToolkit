// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Probes/PCGExProbeDirection.h"

#include "Graph/Probes/PCGExProbing.h"

PCGEX_CREATE_PROBE_FACTORY(Direction, {}, {})

bool UPCGExProbeDirection::PrepareForPoints(const PCGExData::FPointIO* InPointIO)
{
	if (!Super::PrepareForPoints(InPointIO)) { return false; }

	MaxDot = PCGExMath::DegreesToDot(Descriptor.MaxAngle * 0.5);

	if (Descriptor.DirectionSource == EPCGExFetchType::Constant)
	{
		Direction = Descriptor.DirectionConstant;
		bUseConstantDir = true;
	}
	else
	{
		DirectionCache = PrimaryDataCache->GetOrCreateGetter<FVector>(Descriptor.DirectionAttribute);

		if (!DirectionCache)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FText::FromString(TEXT("Invalid Direction attribute: {0}")), FText::FromName(Descriptor.DirectionAttribute.GetName())));
			return false;
		}
	}

	return true;
}

void UPCGExProbeDirection::ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Stacks, const FVector& ST)
{
	bool bIsStacking;
	const double R = SearchRadiusCache ? SearchRadiusCache->Values[Index] : SearchRadiusSquared;
	double BestDot = -1;
	double BestDist = TNumericLimits<double>::Max();
	int32 BestIndex = -1;

	FVector Dir = DirectionCache ? DirectionCache->Values[Index] : Direction;
	if (Descriptor.bTransformDirection) { Dir = Point.Transform.TransformVectorNoScale(Dir); }

	for (const PCGExProbing::FCandidate& C : Candidates)
	{
		if (C.Distance > R) { break; }

		if (Stacks)
		{
			Stacks->Add(C.GH, &bIsStacking);
			if (bIsStacking) { continue; }
		}

		const double Dot = FVector::DotProduct(Dir, C.Direction);

		if (Dot < MaxDot) { continue; }

		if (Dot > BestDot)
		{
			if (C.Distance < BestDist)
			{
				BestDist = C.Distance;
				BestDot = Dot;
				BestIndex = C.PointIndex;
			}
		}
	}

	if (BestIndex != -1) { AddEdge(PCGEx::H64(Index, BestIndex)); }
}

#if WITH_EDITOR
FString UPCGExProbeDirectionProviderSettings::GetDisplayName() const
{
	return TEXT("");
	/*
	return GetDefaultNodeName().ToString()
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Descriptor.WeightFactor) / 1000.0));
		*/
}
#endif
