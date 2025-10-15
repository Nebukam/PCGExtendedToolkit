﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Probes/PCGExProbeTensor.h"


#include "Graph/Probes/PCGExProbing.h"
#include "Transform/Tensors/PCGExTensor.h"
#include "Transform/Tensors/PCGExTensorFactoryProvider.h"
#include "Transform/Tensors/PCGExTensorHandler.h"

TArray<FPCGPinProperties> UPCGExProbeTensorProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_FACTORIES(PCGExTensor::SourceTensorsLabel, "Tensors", Required, FPCGExDataTypeInfoTensor::AsId())
	return PinProperties;
}

PCGEX_CREATE_PROBE_FACTORY(
	Tensor, {}, {
	NewOperation->TensorFactories = &TensorFactories;
	})

PCGExFactories::EPreparationResult UPCGExProbeFactoryTensor::Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
{
	PCGExFactories::EPreparationResult Result = Super::Prepare(InContext, AsyncManager);
	if (Result != PCGExFactories::EPreparationResult::Success) { return Result; }

	if (!PCGExFactories::GetInputFactories(
		InContext, PCGExTensor::SourceTensorsLabel, TensorFactories,
		{PCGExFactories::EType::Tensor}))
	{
		return PCGExFactories::EPreparationResult::Fail;
	}

	if (TensorFactories.IsEmpty())
	{
		PCGEX_LOG_MISSING_INPUT(InContext, FTEXT("Missing tensors."))
		return PCGExFactories::EPreparationResult::Fail;
	}
	return Result;
}

bool FPCGExProbeTensor::RequiresChainProcessing() { return Config.bDoChainedProcessing; }

bool FPCGExProbeTensor::PrepareForPoints(FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIO>& InPointIO)
{
	if (!TensorFactories) { return false; }

	if (!FPCGExProbeOperation::PrepareForPoints(InContext, InPointIO)) { return false; }

	bUseBestDot = (Config.Favor == EPCGExProbeDirectionPriorization::Dot);
	MinDot = PCGExMath::DegreesToDot(Config.MaxAngle);
	Mirror = Config.bInvertTensorDirection ? -1 : 1;

	TensorsHandler = MakeShared<PCGExTensor::FTensorsHandler>(Config.TensorHandlerDetails);
	if (!TensorsHandler->Init(Context, *TensorFactories, PrimaryDataFacade)) { return false; }

	return true;
}

void FPCGExProbeTensor::ProcessCandidates(const int32 Index, const FTransform& WorkingTransform, TArray<PCGExProbing::FCandidate>& Candidates, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges)
{
	bool bIsAlreadyConnected;
	const double R = GetSearchRadius(Index);
	double BestDot = -1;
	double BestDist = MAX_dbl;
	int32 BestCandidateIndex = -1;

	bool bSuccess = false;
	const PCGExTensor::FTensorSample Sample = TensorsHandler->Sample(Index, WorkingTransform, bSuccess);

	if (!bSuccess) { return; }

	const FVector Dir = Sample.DirectionAndSize.GetSafeNormal() * Mirror;
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
		}
		else
		{
			Dot = FVector::DotProduct(Dir, C.Direction);
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

void FPCGExProbeTensor::PrepareBestCandidate(const int32 Index, const FTransform& WorkingTransform, PCGExProbing::FBestCandidate& InBestCandidate)
{
	InBestCandidate.BestIndex = -1;
	InBestCandidate.BestPrimaryValue = -1;
	InBestCandidate.BestSecondaryValue = MAX_dbl;
}

void FPCGExProbeTensor::ProcessCandidateChained(const int32 Index, const FTransform& WorkingTransform, const int32 CandidateIndex, PCGExProbing::FCandidate& Candidate, PCGExProbing::FBestCandidate& InBestCandidate)
{
	const double R = GetSearchRadius(Index);
	bool bSuccess = false;
	const PCGExTensor::FTensorSample Sample = TensorsHandler->Sample(Index, WorkingTransform, bSuccess);

	if (!bSuccess) { return; }

	const FVector Dir = Sample.DirectionAndSize.GetSafeNormal() * Mirror;

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

void FPCGExProbeTensor::ProcessBestCandidate(const int32 Index, const FTransform& WorkingTransform, PCGExProbing::FBestCandidate& InBestCandidate, TArray<PCGExProbing::FCandidate>& Candidates, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges)
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
FString UPCGExProbeTensorProviderSettings::GetDisplayName() const
{
	return TEXT("");
	/*
	return GetDefaultNodeName().ToString()
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.WeightFactor) / 1000.0));
		*/
}
#endif
