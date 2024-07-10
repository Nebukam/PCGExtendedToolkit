// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/Neighbors/PCGExNeighborSampleAttribute.h"

#include "Data/Blending/PCGExMetadataBlender.h"

#define LOCTEXT_NAMESPACE "PCGExCreateNeighborSample"
#define PCGEX_NAMESPACE PCGExCreateNeighborSample

void UPCGExNeighborSampleAttribute::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	const UPCGExNeighborSampleAttribute* TypedOther = Cast<UPCGExNeighborSampleAttribute>(Other);
	if (TypedOther)
	{
		SourceAttributes = TypedOther->SourceAttributes;
		Blending = TypedOther->Blending;
	}
}

void UPCGExNeighborSampleAttribute::PrepareForCluster(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster, PCGExData::FFacade* InVtxDataFacade, PCGExData::FFacade* InEdgeDataFacade)
{
	Super::PrepareForCluster(InContext, InCluster, InVtxDataFacade, InEdgeDataFacade);

	PCGEX_DELETE(Blender)
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
		if (SamplingConfig.NeighborSource == EPCGExGraphValueSource::Vtx) { PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Missing source attribute on vtx: {0}."), FText::FromName(Id))); }
		else { PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Missing source attribute on edges: {0}."), FText::FromName(Id))); }
	}

	if (MetadataBlendingDetails.FilteredAttributes.IsEmpty())
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Missing all source attribute(s) on Sampler {0}."), FText::FromString(GetClass()->GetName())));
		return;
	}

	Blender = new PCGExDataBlending::FMetadataBlender(&MetadataBlendingDetails);
	Blender->bBlendProperties = false;
	Blender->PrepareForData(InVtxDataFacade, GetSourceDataFacade(), PCGExData::ESource::In, true);

	bIsValidOperation = true;
}

void UPCGExNeighborSampleAttribute::FinalizeOperation()
{
	Super::FinalizeOperation();
	PCGEX_DELETE(Blender)
}

void UPCGExNeighborSampleAttribute::Cleanup()
{
	PCGEX_DELETE(Blender)
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

UPCGExNeighborSampleOperation* UPCGExNeighborSamplerFactoryAttribute::CreateOperation() const
{
	UPCGExNeighborSampleAttribute* NewOperation = NewObject<UPCGExNeighborSampleAttribute>();

	PCGEX_SAMPLER_CREATE

	NewOperation->SourceAttributes = Config.SourceAttributes;
	NewOperation->Blending = Config.Blending;

	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExNeighborSampleAttributeSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExNeighborSamplerFactoryAttribute* SamplerFactory = NewObject<UPCGExNeighborSamplerFactoryAttribute>();
	SamplerFactory->Config = Config;

	return Super::CreateFactory(InContext, SamplerFactory);
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
