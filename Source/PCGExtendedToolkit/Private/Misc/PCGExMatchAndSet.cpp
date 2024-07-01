// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExMatchAndSet.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Graph/PCGExCluster.h"
#include "Graph/States/PCGExClusterStates.h"
#include "Misc/MatchAndSet/PCGExMatchAndSetFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE MatchAndSet

int32 UPCGExMatchAndSetSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_M; }

PCGExData::EInit UPCGExMatchAndSetSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

TArray<FPCGPinProperties> UPCGExMatchAndSetSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExMatchAndSet::SourceMatchAndSetsLabel, "Node states.", Required, {})
	PCGEX_PIN_ANY(PCGExMatchAndSet::SourceDefaultsLabel, "Default values that match transmuted attributes when creating new attributes.", Normal, {})
	return PinProperties;
}

FPCGExMatchAndSetContext::~FPCGExMatchAndSetContext()
{
	PCGEX_TERMINATE_ASYNC

	MatchAndSetsFactories.Empty();

	PCGEX_DELETE(DefaultAttributes)
}

PCGEX_INITIALIZE_ELEMENT(MatchAndSet)

bool FPCGExMatchAndSetElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	// Grab all param set attributes

	PCGEX_CONTEXT_AND_SETTINGS(MatchAndSet)

	if (!PCGExFactories::GetInputFactories(
		Context, PCGExMatchAndSet::SourceMatchAndSetsLabel,
		Context->MatchAndSetsFactories, {PCGExFactories::EType::MatchAndSet}, true))
	{
		return false;
	}

	FString Message = TEXT("An unspecified error occured.");
	bool bIsMatchAndSetValid = true;
	PCGEx::FAttributesInfos* ValidationInfos = new PCGEx::FAttributesInfos();
	for (UPCGExMatchAndSetFactoryBase* Factory : Context->MatchAndSetsFactories)
	{
		if (!Factory->AppendAndValidate(ValidationInfos, Message))
		{
			PCGE_LOG(Error, GraphAndLog, FText::FromString(Message));
			bIsMatchAndSetValid = false;
			break;
		}
	}

	PCGEX_DELETE(ValidationInfos)

	if (!bIsMatchAndSetValid) { return false; }

	Context->DefaultAttributes = new PCGEx::FAttributesInfos();

	// TODO : Also check against provided default values

	return true;
}

bool FPCGExMatchAndSetElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMatchAndSetElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(MatchAndSet)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExMatchAndSet::FProcessor>>(
			[](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExMatchAndSet::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any points to process."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->OutputMainPoints();
	Context->Done();

	return Context->TryComplete();
}

namespace PCGExMatchAndSet
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(MatchAndSet)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		return true;
	}

	void FProcessor::CompleteWork()
	{
	}

	void FProcessor::Write()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(MatchAndSet)

		UPCGMetadata* Metadata = PointDataCache->GetOut()->Metadata;

		if (Settings->bDoConsumeProcessedAttributes)
		{
			for (const PCGExData::FCacheBase* DataCache : PointDataCache->Caches)
			{
				if (!DataCache->bIsPureReader ||
					!DataCache->Attribute ||
					!Settings->ConsumeProcessedAttributes.Test(DataCache->Attribute) ||
					PCGEx::IsPCGExAttribute(DataCache->FullName)) { continue; }

				Metadata->DeleteAttribute(DataCache->Attribute->Name);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
