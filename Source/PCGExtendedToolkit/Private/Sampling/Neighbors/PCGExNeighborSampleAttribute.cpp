// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/Neighbors/PCGExNeighborSampleAttribute.h"

#include "Data/Blending/PCGExMetadataBlender.h"


#define LOCTEXT_NAMESPACE "PCGExCreateNeighborSample"
#define PCGEX_NAMESPACE PCGExCreateNeighborSample

void UPCGExNeighborSampleAttribute::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExNeighborSampleAttribute* TypedOther = Cast<UPCGExNeighborSampleAttribute>(Other))
	{
		SourceAttributes = TypedOther->SourceAttributes;
		Blending = TypedOther->Blending;
	}
}

void UPCGExNeighborSampleAttribute::PrepareForCluster(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster> InCluster, const TSharedRef<PCGExData::FFacade> InVtxDataFacade, const TSharedRef<PCGExData::FFacade> InEdgeDataFacade)
{
	Super::PrepareForCluster(InContext, InCluster, InVtxDataFacade, InEdgeDataFacade);

	Blender.Reset();
	bIsValidOperation = false;

	if (SourceAttributes.IsEmpty())
	{
		PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("No source attribute set."));
		return;
	}

	TSet<FName> MissingAttributes;
	PCGExDataBlending::AssembleBlendingDetails(Blending, SourceAttributes, GetSourceIO(), MetadataBlendingDetails, MissingAttributes);

	for (const FName& Id : MissingAttributes)
	{
		if (SamplingConfig.NeighborSource == EPCGExClusterComponentSource::Vtx) { PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Missing source attribute on vtx: {0}."), FText::FromName(Id))); }
		else { PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Missing source attribute on edges: {0}."), FText::FromName(Id))); }
	}

	if (MetadataBlendingDetails.FilteredAttributes.IsEmpty())
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Missing all source attribute(s) on Sampler {0}."), FText::FromString(GetClass()->GetName())));
		return;
	}

	Blender = MakeShared<PCGExDataBlending::FMetadataBlender>(&MetadataBlendingDetails);
	Blender->bBlendProperties = false;
	Blender->PrepareForData(InVtxDataFacade, GetSourceDataFacade(), PCGExData::ESource::In);

	bIsValidOperation = true;
}

void UPCGExNeighborSampleAttribute::CompleteOperation()
{
	Super::CompleteOperation();
	Blender.Reset();
}

void UPCGExNeighborSampleAttribute::Cleanup()
{
	Blender.Reset();
	Super::Cleanup();
}

#if WITH_EDITOR
FString UPCGExNeighborSampleAttributeSettings::GetDisplayName() const
{
	if (Config.SourceAttributes.IsEmpty()) { return TEXT(""); }
	TArray<FName> Names = Config.SourceAttributes.Array();

	if (Names.Num() == 1) { return Names[0].ToString(); }
	if (Names.Num() == 2) { return Names[0].ToString() + TEXT(" (+1 other)"); }

	return Names[0].ToString() + FString::Printf(TEXT(" (+%d others)"), (Names.Num() - 1));
}
#endif

UPCGExNeighborSampleOperation* UPCGExNeighborSamplerFactoryAttribute::CreateOperation(FPCGExContext* InContext) const
{
	UPCGExNeighborSampleAttribute* NewOperation = InContext->ManagedObjects->New<UPCGExNeighborSampleAttribute>();
	PCGEX_SAMPLER_CREATE

	NewOperation->SourceAttributes = Config.SourceAttributes;
	NewOperation->Blending = Config.Blending;

	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExNeighborSampleAttributeSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExNeighborSamplerFactoryAttribute* SamplerFactory = InContext->ManagedObjects->New<UPCGExNeighborSamplerFactoryAttribute>();
	SamplerFactory->Config = Config;

	return Super::CreateFactory(InContext, SamplerFactory);
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
