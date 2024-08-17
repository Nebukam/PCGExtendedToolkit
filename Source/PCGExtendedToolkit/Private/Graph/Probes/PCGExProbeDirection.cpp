// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Probes/PCGExProbeDirection.h"

#include "Graph/Probes/PCGExProbing.h"

PCGEX_CREATE_PROBE_FACTORY(Direction, {}, {})

bool UPCGExProbeDirection::RequiresChainProcessing() { return Config.bDoChainedProcessing; }

bool UPCGExProbeDirection::PrepareForPoints(const PCGExData::FPointIO* InPointIO)
{
	if (!Super::PrepareForPoints(InPointIO)) { return false; }

	bUseBestDot = Config.Favor == EPCGExProbeDirectionPriorization::Dot;
	MaxDot = PCGExMath::DegreesToDot(Config.MaxAngle * 0.5);

	if (Config.DirectionSource == EPCGExFetchType::Constant)
	{
		Direction = Config.DirectionConstant.GetSafeNormal();
		bUseConstantDir = true;
	}
	else
	{
		DirectionCache = PrimaryDataFacade->GetScopedBroadcaster<FVector>(Config.DirectionAttribute);

		if (!DirectionCache)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FText::FromString(TEXT("Invalid Direction attribute: \"{0}\"")), FText::FromName(Config.DirectionAttribute.GetName())));
			return false;
		}
	}

	return true;
}

void UPCGExProbeDirection::ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* ConnectedSet, const FVector& ST, TSet<uint64>* OutEdges)
{
	bool bIsAlreadyConnected;
	const double R = SearchRadiusCache ? SearchRadiusCache->Values[Index] : SearchRadiusSquared;
	double BestDot = -1;
	double BestDist = TNumericLimits<double>::Max();
	int32 BestCandidateIndex = -1;

	FVector Dir = DirectionCache ? DirectionCache->Values[Index].GetSafeNormal() : Direction;
	if (Config.bTransformDirection) { Dir = Point.Transform.TransformVectorNoScale(Dir); }

	for (int i = 0; i < Candidates.Num(); i++)
	{
		const PCGExProbing::FCandidate& C = Candidates[i];

		if (C.Distance > R) { break; }
		if (ConnectedSet && ConnectedSet->Contains(C.GH)) { continue; }
		//if (OutEdges->Contains(PCGEx::H64U(Index, C.PointIndex))) { continue; }

		double Dot = 0;
		if (Config.bUseComponentWiseAngle)
		{
			if (PCGExMath::IsDirectionWithinTolerance(Dir, C.Direction, Config.MaxAngles)) { continue; }
			Dot = FVector::DotProduct(Dir, C.Direction);
		}
		else
		{
			Dot = FVector::DotProduct(Dir, C.Direction);
			if (Dot < MaxDot) { continue; }
		}

		if (bUseBestDot)
		{
			if (Dot >= BestDot)
			{
				if (C.Distance < BestDist)
				{
					BestDist = C.Distance;
					BestDot = Dot;
					BestCandidateIndex = i;
				}
			}
		}
		else if (C.Distance < BestDist)
		{
			BestDist = C.Distance;
			BestDot = Dot;
			BestCandidateIndex = i;
		}
	}

	if (BestCandidateIndex != -1)
	{
		const PCGExProbing::FCandidate& C = Candidates[BestCandidateIndex];

		if (ConnectedSet)
		{
			ConnectedSet->Add(C.GH, &bIsAlreadyConnected);
			if (bIsAlreadyConnected) { return; }
		}

		OutEdges->Add(PCGEx::H64U(Index, C.PointIndex));
	}
}

void UPCGExProbeDirection::PrepareBestCandidate(const int32 Index, const FPCGPoint& Point, PCGExProbing::FBestCandidate& InBestCandidate)
{
	InBestCandidate.BestIndex = -1;
	InBestCandidate.BestPrimaryValue = -1;
	InBestCandidate.BestSecondaryValue = TNumericLimits<double>::Max();
}

void UPCGExProbeDirection::ProcessCandidateChained(const int32 Index, const FPCGPoint& Point, const int32 CandidateIndex, PCGExProbing::FCandidate& Candidate, PCGExProbing::FBestCandidate& InBestCandidate)
{
	const double R = SearchRadiusCache ? SearchRadiusCache->Values[Index] : SearchRadiusSquared;
	FVector Dir = DirectionCache ? DirectionCache->Values[Index].GetSafeNormal() : Direction;
	if (Config.bTransformDirection) { Dir = Point.Transform.TransformVectorNoScale(Dir); }

	if (Candidate.Distance > R) { return; }

	double Dot = 0;
	if (Config.bUseComponentWiseAngle)
	{
		if (PCGExMath::IsDirectionWithinTolerance(Dir, Candidate.Direction, Config.MaxAngles)) { return; }
		Dot = FVector::DotProduct(Dir, Candidate.Direction);
	}
	else
	{
		Dot = FVector::DotProduct(Dir, Candidate.Direction);
		if (Dot < MaxDot) { return; }
	}

	if (bUseBestDot)
	{
		if (Dot >= InBestCandidate.BestPrimaryValue)
		{
			if (Candidate.Distance < InBestCandidate.BestSecondaryValue)
			{
				InBestCandidate.BestSecondaryValue = Candidate.Distance;
				InBestCandidate.BestPrimaryValue = Dot;
				InBestCandidate.BestIndex = CandidateIndex;
			}
		}
	}
	else if (Candidate.Distance < InBestCandidate.BestSecondaryValue)
	{
		InBestCandidate.BestSecondaryValue = Candidate.Distance;
		InBestCandidate.BestPrimaryValue = Dot;
		InBestCandidate.BestIndex = CandidateIndex;
	}
}

void UPCGExProbeDirection::ProcessBestCandidate(const int32 Index, const FPCGPoint& Point, PCGExProbing::FBestCandidate& InBestCandidate, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Stacks, const FVector& ST, TSet<uint64>* OutEdges)
{
	if (InBestCandidate.BestIndex == -1) { return; }

	const PCGExProbing::FCandidate& C = Candidates[InBestCandidate.BestIndex];

	bool bIsAlreadyConnected;
	if (Stacks)
	{
		Stacks->Add(C.GH, &bIsAlreadyConnected);
		if (bIsAlreadyConnected) { return; }
	}

	OutEdges->Add(PCGEx::H64U(Index, C.PointIndex));
}

#if WITH_EDITOR
FString UPCGExProbeDirectionProviderSettings::GetDisplayName() const
{
	return TEXT("");
	/*
	return GetDefaultNodeName().ToString()
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.WeightFactor) / 1000.0));
		*/
}
#endif
