// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Core/Samplers/PCGExTensorSampler.h"

#include "Core/PCGExTensorOperation.h"


void UPCGExTensorSampler::CopySettingsFrom(const UPCGExInstancedFactory* Other)
{
	Super::CopySettingsFrom(Other);
}

bool UPCGExTensorSampler::PrepareForData(FPCGExContext* InContext)
{
	return true;
}

PCGExTensor::FTensorSample UPCGExTensorSampler::RawSample(const TArray<TSharedPtr<PCGExTensorOperation>>& InTensors, const int32 InSeedIndex, const FTransform& InProbe) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExTensorSampler::RawSample);

	PCGExTensor::FTensorSample Result = PCGExTensor::FTensorSample();

	TArray<PCGExTensor::FTensorSample> Samples;
	double TotalWeight = 0;

	FVector WeightedDirectionAndSize = FVector::ZeroVector;
	FQuat WeightedRotation = FQuat::Identity;
	double CumulativeWeight = 0.0f;

	for (const TSharedPtr<PCGExTensorOperation>& Op : InTensors)
	{
		const PCGExTensor::FTensorSample Sample = Op->Sample(InSeedIndex, InProbe);
		if (Sample.Effectors == 0) { continue; }
		Result.Effectors += Sample.Effectors;
		Samples.Add(Sample);
		TotalWeight += Sample.Weight;
	}

	for (int i = 0; i < Samples.Num(); i++)
	{
		const PCGExTensor::FTensorSample& Sample = Samples[i];
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

PCGExTensor::FTensorSample UPCGExTensorSampler::Sample(const TArray<TSharedPtr<PCGExTensorOperation>>& InTensors, const int32 InSeedIndex, const FTransform& InProbe, bool& OutSuccess) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExTensorSampler::Sample);

	const PCGExTensor::FTensorSample Result = RawSample(InTensors, InSeedIndex, InProbe);
	OutSuccess = Result.Effectors > 0;
	return Result;
}
