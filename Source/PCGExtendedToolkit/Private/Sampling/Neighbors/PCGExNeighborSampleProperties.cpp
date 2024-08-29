// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/Neighbors/PCGExNeighborSampleProperties.h"

#include "Data/Blending/PCGExPropertiesBlender.h"

#define LOCTEXT_NAMESPACE "PCGExCreateNeighborSample"
#define PCGEX_NAMESPACE PCGExCreateNeighborSample

void UPCGExNeighborSampleProperties::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExNeighborSampleProperties* TypedOther = Cast<UPCGExNeighborSampleProperties>(Other))
	{
		BlendingDetails = TypedOther->BlendingDetails;
	}
}

void UPCGExNeighborSampleProperties::PrepareForCluster(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster, PCGExData::FFacade* InVtxDataFacade, PCGExData::FFacade* InEdgeDataFacade)
{
	PCGEX_DELETE(Blender)
	Blender = new PCGExDataBlending::FPropertiesBlender(BlendingDetails);
	return Super::PrepareForCluster(InContext, InCluster, InVtxDataFacade, InEdgeDataFacade);
}

void UPCGExNeighborSampleProperties::Cleanup()
{
	PCGEX_DELETE(Blender)
	Super::Cleanup();
}

#if WITH_EDITOR
FString UPCGExNeighborSamplePropertiesSettings::GetDisplayName() const
{
	if (Config.Blending.HasNoBlending()) { return TEXT("(None)"); }
	TArray<FName> Names;
	Config.Blending.GetNonNoneBlendings(Names);

	if (Names.Num() == 1) { return Names[0].ToString(); }
	if (Names.Num() == 2) { return Names[0].ToString() + TEXT(" (+1 other)"); }

	return Names[0].ToString() + FString::Printf(TEXT(" (+%d others)"), (Names.Num() - 1));
}
#endif

UPCGExNeighborSampleOperation* UPCGExNeighborSamplerFactoryProperties::CreateOperation() const
{
	PCGEX_NEW_TRANSIENT(UPCGExNeighborSampleProperties, NewOperation)
	PCGEX_SAMPLER_CREATE

	NewOperation->BlendingDetails = Config.Blending;

	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExNeighborSamplePropertiesSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExNeighborSamplerFactoryProperties* SamplerFactory = NewObject<UPCGExNeighborSamplerFactoryProperties>();
	SamplerFactory->Config = Config;

	return Super::CreateFactory(InContext, SamplerFactory);
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
