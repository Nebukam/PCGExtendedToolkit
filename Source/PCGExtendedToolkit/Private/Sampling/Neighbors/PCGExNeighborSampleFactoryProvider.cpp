// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/Neighbors/PCGExNeighborSampleFactoryProvider.h"

#include "Sampling/Neighbors/PCGExNeighborSampleOperation.h"

#define LOCTEXT_NAMESPACE "PCGExCreateNeighborSample"
#define PCGEX_NAMESPACE PCGExCreateNeighborSample

#if WITH_EDITOR
FString UPCGExNeighborSampleProviderSettings::GetDisplayName() const
{
	return SamplerSettings.SourceAttribute.ToString();
}
#endif

UPCGExNeighborSampleOperation* UPCGNeighborSamplerFactoryBase::CreateOperation() const
{
	UPCGExNeighborSampleOperation* NewOperation = NewObject<UPCGExNeighborSampleOperation>();

	NewOperation->RangeType = Descriptor.RangeType;
	NewOperation->BlendOver = Descriptor.BlendOver;
	NewOperation->MaxDepth = Descriptor.MaxDepth;
	NewOperation->MaxDistance = Descriptor.MaxDistance;
	NewOperation->FixedBlend = Descriptor.FixedBlend;
	PCGEX_LOAD_SOFTOBJECT(UCurveFloat, Descriptor.WeightOverDistanceCurve, NewOperation->WeightCurveObj, PCGEx::WeightDistributionLinear)
	NewOperation->NeighborSource = Descriptor.NeighborSource;
	NewOperation->SourceAttribute = Descriptor.SourceAttribute;
	NewOperation->bOutputToNewAttribute = Descriptor.bOutputToNewAttribute;
	NewOperation->TargetAttribute = Descriptor.TargetAttribute;
	NewOperation->Blending = Descriptor.Blending;

	return NewOperation;
}

FName UPCGExNeighborSampleProviderSettings::GetMainOutputLabel() const { return PCGExNeighborSample::OutputSamplerLabel; }

UPCGExParamFactoryBase* UPCGExNeighborSampleProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	if (!InFactory)
	{
		InFactory = NewObject<UPCGNeighborSamplerFactoryBase>();
	}

	UPCGNeighborSamplerFactoryBase* NewFactory = Cast<UPCGNeighborSamplerFactoryBase>(InFactory);
	NewFactory->Descriptor = SamplerSettings;

	return InFactory;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
