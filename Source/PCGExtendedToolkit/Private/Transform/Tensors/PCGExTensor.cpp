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

	FTensorSample FTensorsHandler::Sample(const FTransform& InProbe, bool& OutSuccess) const
	{
		FTensorSample Result = FTensorSample();

		TArray<FTensorSample> Samples;
		double TotalWeight = 0;

		FVector WeightedDirectionAndSize = FVector::ZeroVector;
		FQuat WeightedRotation = FQuat::Identity;
		double CumulativeWeight = 0.0f;

		for (const UPCGExTensorOperation* Op : Operations)
		{
			const FTensorSample Sample = Op->Sample(InProbe);
			if (Sample.Effectors == 0) { continue; }
			Result.Effectors += Sample.Effectors;
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

}

void FPCGExTensorConfigBase::Init()
{
	PCGEX_MAKE_SHARED(CurvePaths, TSet<FSoftObjectPath>)

	if (!bUseLocalWeightFalloffCurve) { CurvePaths->Add(WeightFalloffCurve.ToSoftObjectPath()); }
	if (!bUseLocalPotencyFalloffCurve) { CurvePaths->Add(PotencyFalloffCurve.ToSoftObjectPath()); }
	if (!bUseLocalGuideCurve) { CurvePaths->Add(GuideCurve.ToSoftObjectPath()); }

	PCGExHelpers::LoadBlocking_AnyThread(CurvePaths);

	LocalWeightFalloffCurve.ExternalCurve = WeightFalloffCurve.Get();
	WeightFalloffCurveObj = LocalWeightFalloffCurve.GetRichCurveConst();

	LocalPotencyFalloffCurve.ExternalCurve = PotencyFalloffCurve.Get();
	PotencyFalloffCurveObj = LocalPotencyFalloffCurve.GetRichCurveConst();

	LocalGuideCurve.ExternalCurve = GuideCurve.Get();
}
