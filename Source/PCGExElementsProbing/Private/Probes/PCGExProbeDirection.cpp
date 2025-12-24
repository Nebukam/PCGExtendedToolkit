// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Probes/PCGExProbeDirection.h"

#include "PCGExH.h"
#include "Containers/PCGExManagedObjects.h"
#include "Details/PCGExSettingsDetails.h"
#include "Core/PCGExProbingCandidates.h"
#include "Math/PCGExMath.h"

PCGEX_SETTING_VALUE_IMPL(FPCGExProbeConfigDirection, Direction, FVector, DirectionInput, DirectionAttribute, DirectionConstant)
PCGEX_CREATE_PROBE_FACTORY(Direction, {}, {})

bool FPCGExProbeDirection::RequiresChainProcessing() { return Config.bDoChainedProcessing; }

bool FPCGExProbeDirection::PrepareForPoints(FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIO>& InPointIO)
{
	if (!FPCGExProbeOperation::PrepareForPoints(InContext, InPointIO)) { return false; }

	bUseBestDot = Config.Favor == EPCGExProbeDirectionPriorization::Dot;
	MinDot = PCGExMath::DegreesToDot(Config.MaxAngle);
	DirectionMultiplier = Config.bInvertDirection ? -1 : 1;

	Direction = Config.GetValueSettingDirection();
	if (!Direction->Init(PrimaryDataFacade)) { return false; }

	return true;
}

#define PCGEX_GET_DIRECTION (Direction->Read(Index) * DirectionMultiplier).GetSafeNormal()

void FPCGExProbeDirection::ProcessCandidates(const int32 Index, const FTransform& WorkingTransform, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, PCGExMT::FScopedContainer* Container)
{
	bool bIsAlreadyConnected;
	const double R = GetSearchRadius(Index);
	double BestDot = -1;
	double BestDist = MAX_dbl;
	int32 BestCandidateIndex = -1;

	const FVector Dir = Config.bTransformDirection ? WorkingTransform.TransformVectorNoScale(PCGEX_GET_DIRECTION) : PCGEX_GET_DIRECTION;

	const int32 MaxIndex = Candidates.Num() - 1;
	for (int i = 0; i <= MaxIndex; i++)
	{
		const int32 LocalIndex = bUseBestDot ? MaxIndex - i : i;
		const PCGExProbing::FCandidate& C = Candidates[LocalIndex];

		// When using best dot, we need to process the candidates backward, so can't break the loop.
		if (bUseBestDot) { if (C.Distance > R) { continue; } }
		else { if (C.Distance > R) { break; } }

		if (Coincidence && Coincidence->Contains(C.GH)) { continue; }

		double Dot = 0;
		if (Config.bUseComponentWiseAngle)
		{
			if (PCGExMath::IsDirectionWithinTolerance(Dir, C.Direction, Config.MaxAngles)) { continue; }
			Dot = FVector::DotProduct(Dir, C.Direction);
			if (Config.bUnsignedCheck) { Dot = FMath::Abs(Dot); }
		}
		else
		{
			Dot = FVector::DotProduct(Dir, C.Direction);
			if (Config.bUnsignedCheck) { Dot = FMath::Abs(Dot); }
			if (Dot < MinDot) { continue; }
		}

		if (Dot >= BestDot)
		{
			if (C.Distance < BestDist)
			{
				BestDist = C.Distance;
				BestDot = Dot;
				BestCandidateIndex = LocalIndex;
			}
		}
	}

	if (BestCandidateIndex != -1)
	{
		const PCGExProbing::FCandidate& C = Candidates[BestCandidateIndex];

		if (Coincidence)
		{
			Coincidence->Add(C.GH, &bIsAlreadyConnected);
			if (bIsAlreadyConnected) { return; }
		}

		OutEdges->Add(PCGEx::H64U(Index, C.PointIndex));
	}
}

void FPCGExProbeDirection::PrepareBestCandidate(const int32 Index, const FTransform& WorkingTransform, PCGExProbing::FBestCandidate& InBestCandidate, PCGExMT::FScopedContainer* Container)
{
	InBestCandidate.BestIndex = -1;
	InBestCandidate.BestPrimaryValue = -1;
	InBestCandidate.BestSecondaryValue = MAX_dbl;
}

void FPCGExProbeDirection::ProcessCandidateChained(const int32 Index, const FTransform& WorkingTransform, const int32 CandidateIndex, PCGExProbing::FCandidate& Candidate, PCGExProbing::FBestCandidate& InBestCandidate, PCGExMT::FScopedContainer* Container)
{
	const double R = GetSearchRadius(Index);
	const FVector Dir = Config.bTransformDirection ? WorkingTransform.TransformVectorNoScale(PCGEX_GET_DIRECTION) : PCGEX_GET_DIRECTION;

	if (Candidate.Distance > R) { return; }

	double Dot = 0;
	if (Config.bUseComponentWiseAngle)
	{
		if (PCGExMath::IsDirectionWithinTolerance(Dir, Candidate.Direction, Config.MaxAngles)) { return; }
		Dot = FVector::DotProduct(Dir, Candidate.Direction);
		if (Config.bUnsignedCheck) { Dot = FMath::Abs(Dot); }
	}
	else
	{
		Dot = FVector::DotProduct(Dir, Candidate.Direction);
		if (Config.bUnsignedCheck) { Dot = FMath::Abs(Dot); }
		if (Dot < MinDot) { return; }
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

#undef PCGEX_GET_DIRECTION

void FPCGExProbeDirection::ProcessBestCandidate(const int32 Index, const FTransform& WorkingTransform, PCGExProbing::FBestCandidate& InBestCandidate, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, PCGExMT::FScopedContainer* Container)
{
	if (InBestCandidate.BestIndex == -1) { return; }

	const PCGExProbing::FCandidate& C = Candidates[InBestCandidate.BestIndex];

	bool bIsAlreadyConnected;
	if (Coincidence)
	{
		Coincidence->Add(C.GH, &bIsAlreadyConnected);
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
