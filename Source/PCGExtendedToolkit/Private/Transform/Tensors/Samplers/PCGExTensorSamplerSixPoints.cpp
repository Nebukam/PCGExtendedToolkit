// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Transform/Tensors/Samplers/PCGExTensorSamplerSixPoints.h"

#include "Transform/Tensors/PCGExTensorOperation.h"


void UPCGExTensorSamplerSixPoints::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
}

bool UPCGExTensorSamplerSixPoints::PrepareForData(FPCGExContext* InContext)
{
	return true;
}

PCGExTensor::FTensorSample UPCGExTensorSamplerSixPoints::Sample(const TArray<UPCGExTensorOperation*>& InTensors, const FTransform& InProbe, bool& OutSuccess) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExTensorSamplerSixPoints::Sample);

	PCGExTensor::FTensorSample Result = PCGExTensor::FTensorSample();
	for (int i = 0; i < 6; i++)
	{
		FTransform PointProbe = InProbe;
		PointProbe.AddToTranslation(Points[i] * Radius);
		Result += Super::RawSample(InTensors, PointProbe);
	}

	Result /= 6;
	OutSuccess = Result.Effectors > 0;

	return Result;
}
