// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Core/Samplers/PCGExTensorSamplerSixPoints.h"

void UPCGExTensorSamplerSixPoints::CopySettingsFrom(const UPCGExInstancedFactory* Other)
{
	Super::CopySettingsFrom(Other);
}

bool UPCGExTensorSamplerSixPoints::PrepareForData(FPCGExContext* InContext)
{
	return true;
}

PCGExTensor::FTensorSample UPCGExTensorSamplerSixPoints::Sample(const TArray<TSharedPtr<PCGExTensorOperation>>& InTensors, const int32 InSeedIndex, const FTransform& InProbe, bool& OutSuccess) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExTensorSamplerSixPoints::Sample);

	PCGExTensor::FTensorSample Result = PCGExTensor::FTensorSample();
	for (int i = 0; i < 6; i++)
	{
		FTransform PointProbe = InProbe;
		PointProbe.AddToTranslation(Points[i] * Radius);
		Result += Super::RawSample(InTensors, InSeedIndex, PointProbe);
	}

	Result /= 6;
	OutSuccess = Result.Effectors > 0;

	return Result;
}
