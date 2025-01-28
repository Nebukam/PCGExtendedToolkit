// Copyright 2025 Timothé Lapetite and contributors
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

	TArray<FName> SourceNames;
	SourceAttributes.GetSources(SourceNames);

	PCGExDataBlending::AssembleBlendingDetails(Blending, SourceNames, GetSourceIO(), MetadataBlendingDetails, MissingAttributes);

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

	SourceAttributes.SetOutputTargetNames(InVtxDataFacade);

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
	TArray<FName> SourceNames;
	Config.SourceAttributes.GetSources(SourceNames);

	if (SourceNames.Num() == 1) { return SourceNames[0].ToString(); }
	if (SourceNames.Num() == 2) { return SourceNames[0].ToString() + TEXT(" (+1 other)"); }

	return SourceNames[0].ToString() + FString::Printf(TEXT(" (+%d others)"), (SourceNames.Num() - 1));
}
#endif

UPCGExNeighborSampleOperation* UPCGExNeighborSamplerFactoryAttribute::CreateOperation(FPCGExContext* InContext) const
{
	UPCGExNeighborSampleAttribute* NewOperation = InContext->ManagedObjects->New<UPCGExNeighborSampleAttribute>();

	PCGEX_SAMPLER_CREATE_OPERATION

	NewOperation->SourceAttributes = Config.SourceAttributes;
	NewOperation->Blending = Config.Blending;

	return NewOperation;
}

bool UPCGExNeighborSamplerFactoryAttribute::RegisterConsumableAttributes(FPCGExContext* InContext) const
{
	if (!Super::RegisterConsumableAttributes(InContext)) { return false; }
	for (const FPCGExAttributeSourceToTargetDetails& Entry : Config.SourceAttributes.Attributes)
	{
		if (Entry.bOutputToDifferentName) { InContext->AddConsumableAttributeName(Entry.Source); }
	}
	return true;
}

void UPCGExNeighborSamplerFactoryAttribute::RegisterVtxBuffersDependencies(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterVtxBuffersDependencies(InContext, InVtxDataFacade, FacadePreloader);

	if (SamplingConfig.NeighborSource == EPCGExClusterComponentSource::Vtx)
	{
		TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(InVtxDataFacade->GetIn()->Metadata);

		TArray<FName> SourceNames;
		Config.SourceAttributes.GetSources(SourceNames);

		for (const FName AttrName : SourceNames)
		{
			const PCGEx::FAttributeIdentity* Identity = Infos->Find(AttrName);
			if (!Identity)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Missing attribute: \"{0}\"."), FText::FromName(AttrName)));
				return;
			}
			FacadePreloader.Register(InContext, *Identity);
		}
	}
}

UPCGExFactoryData* UPCGExNeighborSampleAttributeSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	if (!Config.SourceAttributes.ValidateNames(InContext)) { return nullptr; }

	UPCGExNeighborSamplerFactoryAttribute* SamplerFactory = InContext->ManagedObjects->New<UPCGExNeighborSamplerFactoryAttribute>();
	SamplerFactory->Config = Config;

	return Super::CreateFactory(InContext, SamplerFactory);
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
