// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/Neighbors/PCGExNeighborSampleFactoryProvider.h"

#include "Sampling/Neighbors/PCGExNeighborSampleOperation.h"

#define LOCTEXT_NAMESPACE "PCGExCreateNeighborSample"
#define PCGEX_NAMESPACE PCGExCreateNeighborSample

#if WITH_EDITOR
FString UPCGExNeighborSampleProviderSettings::GetDisplayName() const
{
	if (SamplerSettings.SourceAttributes.IsEmpty()) { return TEXT(""); }
	TArray<FName> Names = SamplerSettings.SourceAttributes.Array();

	if (Names.Num() == 1) { return Names[0].ToString(); }
	if (Names.Num() == 2) { return Names[0].ToString() + TEXT(" (+1 other)"); }

	return Names[0].ToString() + FString::Printf(TEXT(" (+%d others)"), (Names.Num() - 1));
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
	PCGEX_LOAD_SOFTOBJECT(UCurveFloat, Descriptor.WeightCurve, NewOperation->WeightCurveObj, PCGEx::WeightDistributionLinear)
	NewOperation->NeighborSource = Descriptor.NeighborSource;
	NewOperation->SourceAttributes = Descriptor.SourceAttributes;
	//NewOperation->bOutputToNewAttribute = Descriptor.bOutputToNewAttribute;
	//NewOperation->TargetAttribute = Descriptor.TargetAttribute;
	NewOperation->Blending = Descriptor.Blending;

	if (!FilterFactories.IsEmpty()) { NewOperation->PointState = new PCGExCluster::FNodeStateHandler(this); }
	if (ValueStateFactory) { NewOperation->ValueState = new PCGExCluster::FNodeStateHandler(ValueStateFactory); }

	return NewOperation;
}

TArray<FPCGPinProperties> UPCGExNeighborSampleProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGEx::SourcePointFilters, "Filters used to check which node will be processed by the sampler or not.", Advanced, {})
	PCGEX_PIN_PARAMS(PCGEx::SourceUseValueIfFilters, "Filters used to check if a node can be used as a value source or not.", Advanced, {})
	return PinProperties;
}

FName UPCGExNeighborSampleProviderSettings::GetMainOutputLabel() const { return PCGExNeighborSample::OutputSamplerLabel; }

UPCGExParamFactoryBase* UPCGExNeighborSampleProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	if (!InFactory)
	{
		InFactory = NewObject<UPCGNeighborSamplerFactoryBase>();
	}

	UPCGNeighborSamplerFactoryBase* SamplerFactory = Cast<UPCGNeighborSamplerFactoryBase>(InFactory);
	SamplerFactory->Descriptor = SamplerSettings;
	PCGExDataFilter::GetInputFactories(InContext, PCGEx::SourcePointFilters, SamplerFactory->FilterFactories,  PCGExFactories::ClusterFilters, false);

	TArray<UPCGExFilterFactoryBase*> FilterFactories;
	if (PCGExDataFilter::GetInputFactories(InContext, PCGEx::SourceUseValueIfFilters, FilterFactories, PCGExFactories::ClusterFilters, false))
	{
		SamplerFactory->ValueStateFactory = NewObject<UPCGExNodeStateFactory>();
		SamplerFactory->ValueStateFactory->FilterFactories.Append(FilterFactories);
	}

	return InFactory;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
