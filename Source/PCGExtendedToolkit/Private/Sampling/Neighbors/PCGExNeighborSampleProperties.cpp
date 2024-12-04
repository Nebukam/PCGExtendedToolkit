// Copyright 2024 Timothé Lapetite and contributors
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

void UPCGExNeighborSampleProperties::PrepareForCluster(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster> InCluster, const TSharedRef<PCGExData::FFacade> InVtxDataFacade, const TSharedRef<PCGExData::FFacade> InEdgeDataFacade)
{
	PropertiesBlender = MakeUnique<PCGExDataBlending::FPropertiesBlender>(BlendingDetails);
	return Super::PrepareForCluster(InContext, InCluster, InVtxDataFacade, InEdgeDataFacade);
}

void UPCGExNeighborSampleProperties::Cleanup()
{
	PropertiesBlender.Reset();
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

UPCGExNeighborSampleOperation* UPCGExNeighborSamplerFactoryProperties::CreateOperation(FPCGExContext* InContext) const
{
	UPCGExNeighborSampleProperties* NewOperation = InContext->ManagedObjects->New<UPCGExNeighborSampleProperties>();
	PCGEX_SAMPLER_CREATE

	NewOperation->BlendingDetails = Config.Blending;

	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExNeighborSamplePropertiesSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExNeighborSamplerFactoryProperties* SamplerFactory = InContext->ManagedObjects->New<UPCGExNeighborSamplerFactoryProperties>();
	SamplerFactory->Config = Config;

	return Super::CreateFactory(InContext, SamplerFactory);
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
