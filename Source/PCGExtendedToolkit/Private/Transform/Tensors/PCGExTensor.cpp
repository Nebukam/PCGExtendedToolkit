// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensor.h"

#include "Transform/Tensors/PCGExTensorFactoryProvider.h"
#include "Transform/Tensors/PCGExTensorOperation.h"

namespace PCGExTensor
{
	FEffectorSample& FEffectorSamples::Emplace_GetRef(const FVector& InDirection, const double InPotency, const double InWeight)
	{
		TotalPotency += InPotency;
		TensorSample.Weight += InWeight;
		return Samples.Emplace_GetRef(InDirection, InPotency, InWeight);
	}

	FTensorSample FEffectorSamples::Flatten(const double InWeight)
	{
		TensorSample.Effectors = Samples.Num();

		FVector DirectionAndSize = FVector::ZeroVector;

		for (const FEffectorSample& EffectorSample : Samples)
		{
			const double S = (EffectorSample.Potency * (EffectorSample.Weight / TensorSample.Weight));
			DirectionAndSize += EffectorSample.Direction * S;
		}

		TensorSample.DirectionAndSize = DirectionAndSize;
		TensorSample.Rotation = FRotationMatrix::MakeFromX(DirectionAndSize.GetSafeNormal()).ToQuat();
		TensorSample.Weight = InWeight;

		return TensorSample;
	}

	FTensorsHandler::FTensorsHandler()
	{
	}

	bool FTensorsHandler::Init(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExTensorFactoryData>>& InFactories)
	{
		Operations.Reserve(InFactories.Num());

		for (const UPCGExTensorFactoryData* Factory : InFactories)
		{
			UPCGExTensorOperation* Op = Factory->CreateOperation(InContext);
			Operations.Add(Op);
		}

		return true;
	}

	bool FTensorsHandler::Init(FPCGExContext* InContext, const FName InPin)
	{
		TArray<TObjectPtr<const UPCGExTensorFactoryData>> InFactories;
		if (!PCGExFactories::GetInputFactories(InContext, InPin, InFactories, {PCGExFactories::EType::Tensor}, true)) { return false; }
		if (InFactories.IsEmpty())
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Missing tensors."));
			return false;
		}
		return Init(InContext, InFactories);
	}

	FTensorSample FTensorsHandler::SampleAtPosition(const FVector& InPosition, bool& OutSuccess) const
	{
		FTensorSample Result = FTensorSample();

		TArray<FTensorSample> Samples;
		double TotalWeight = 0;

		FVector WeightedDirectionAndSize = FVector::ZeroVector;
		FQuat WeightedRotation = FQuat::Identity;
		double CumulativeWeight = 0.0f;

		for (const UPCGExTensorOperation* Op : Operations)
		{
			const FTensorSample Sample = Op->SampleAtPosition(InPosition);
			if (Sample.Effectors == 0) { continue; }
			Samples.Add(Sample);
			TotalWeight += Sample.Weight;
		}

		OutSuccess = Samples.Num() > 0;

		for (int i = 0; i < Samples.Num(); i++)
		{
			const FTensorSample& Sample = Samples[i];
			const double W = Sample.Weight / TotalWeight;
			WeightedDirectionAndSize += Sample.DirectionAndSize * W;

			if (i == 0)
			{
				WeightedRotation = Sample.Rotation;
				CumulativeWeight = W;
			}
			else
			{
				WeightedRotation = FQuat::Slerp(WeightedRotation, Sample.Rotation, W / (CumulativeWeight + W));
				CumulativeWeight += W;
			}
		}

		WeightedRotation.Normalize();

		Result.DirectionAndSize = WeightedDirectionAndSize;
		Result.Rotation = WeightedRotation;

		return Result;
	}

	FTensorSample FTensorsHandler::SampleAtPositionOrderedInPlace(const FVector& InPosition, bool& OutSuccess) const
	{
		// TODO : Go through all operations and gather samples, apply them one after another
		OutSuccess = false;
		return FTensorSample{};
	}

	FTensorSample FTensorsHandler::SampleAtPositionOrderedMutated(const FVector& InPosition, bool& OutSuccess) const
	{
		FVector UpdatedPosition = InPosition;
		// TODO : Go through all operations and gather samples, apply them & update sampling position one after another
		OutSuccess = false;
		return FTensorSample{};
	}
}

void FPCGExTensorConfigBase::Init()
{
	if (!bUseLocalWeightFalloffCurve)
	{
		PCGExHelpers::LoadBlocking_AnyThread(WeightFalloffCurve);
		LocalWeightFalloffCurve.ExternalCurve = WeightFalloffCurve.Get();
	}
	WeightFalloffCurveObj = LocalWeightFalloffCurve.GetRichCurveConst();

	if (!bUseLocalPotencyFalloffCurve)
	{
		PCGExHelpers::LoadBlocking_AnyThread(PotencyFalloffCurve);
		LocalPotencyFalloffCurve.ExternalCurve = PotencyFalloffCurve.Get();
	}
	PotencyFalloffCurveObj = LocalPotencyFalloffCurve.GetRichCurveConst();
}
