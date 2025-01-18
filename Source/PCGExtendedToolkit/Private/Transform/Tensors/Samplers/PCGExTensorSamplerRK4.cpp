// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Transform/Tensors/Samplers/PCGExTensorSamplerRK4.h"

#include "Transform/Tensors/PCGExTensorOperation.h"


void UPCGExTensorSamplerRK4::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
}

bool UPCGExTensorSamplerRK4::PrepareForData(FPCGExContext* InContext)
{
	return true;
}

PCGExTensor::FTensorSample UPCGExTensorSamplerRK4::Sample(const TArray<UPCGExTensorOperation*>& InTensors, const FTransform& InProbe, bool& OutSuccess) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExTensorSamplerRK4::Sample);

	PCGExTensor::FTensorSample Result = PCGExTensor::FTensorSample();
	OutSuccess = true;

	PCGExTensor::FTensorSample K1 = Super::RawSample(InTensors, InProbe);
	Result += K1;

	PCGExTensor::FTensorSample K2 = Super::RawSample(InTensors, K1.GetTransformed(InProbe, (Radius * 0.5)));
	Result += K2;

	PCGExTensor::FTensorSample K3 = Super::RawSample(InTensors, K2.GetTransformed(InProbe, (Radius * 0.5)));
	Result += K3;

	PCGExTensor::FTensorSample K4 = Super::RawSample(InTensors, K3.GetTransformed(InProbe, Radius));
	Result += K4;

	Result.DirectionAndSize = (Radius / 6.0f) * (K1.DirectionAndSize + 2.0f * K2.DirectionAndSize + 2.0f * K3.DirectionAndSize + K4.DirectionAndSize);
	OutSuccess = Result.Effectors > 0;

	return Result;
}
