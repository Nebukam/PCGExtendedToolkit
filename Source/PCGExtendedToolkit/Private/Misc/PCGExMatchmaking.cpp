// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExMatchmaking.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Graph/PCGExCluster.h"
#include "Graph/States/PCGExClusterStates.h"
#include "Misc/Matchmakers/PCGExMatchToFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE Matchmaking

int32 UPCGExMatchmakingSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_M; }

PCGExData::EInit UPCGExMatchmakingSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

TArray<FPCGPinProperties> UPCGExMatchmakingSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExMatchmaking::SourceMatchmakersLabel, "Matchmakers nodes.", Required, {})
	PCGEX_PIN_ANY(PCGExMatchmaking::SourceDefaultsLabel, "Default values that match attributes when creating new attributes through matchmaking.", Normal, {})
	return PinProperties;
}

FPCGExMatchmakingContext::~FPCGExMatchmakingContext()
{
	PCGEX_TERMINATE_ASYNC

	MatchmakingsFactories.Empty();

	PCGEX_DELETE(DefaultAttributes)
}

PCGEX_INITIALIZE_ELEMENT(Matchmaking)

bool FPCGExMatchmakingElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	// Grab all param set attributes

	PCGEX_CONTEXT_AND_SETTINGS(Matchmaking)

	if (!PCGExFactories::GetInputFactories(
		Context, PCGExMatchmaking::SourceMatchmakersLabel, Context->MatchmakingsFactories,
		{PCGExFactories::EType::Matchmaking}, true))
	{
		return false;
	}

	FString Message = TEXT("An unspecified error occured.");
	bool bIsMatchmakingValid = true;
	PCGEx::FAttributesInfos* ValidationInfos = new PCGEx::FAttributesInfos();
	for (UPCGExMatchToFactoryBase* Factory : Context->MatchmakingsFactories)
	{
		if (!Factory->AppendAndValidate(ValidationInfos, Message))
		{
			PCGE_LOG(Error, GraphAndLog, FText::FromString(Message));
			bIsMatchmakingValid = false;
			break;
		}
	}

	PCGEX_DELETE(ValidationInfos)

	if (!bIsMatchmakingValid) { return false; }

	Context->DefaultAttributes = new PCGEx::FAttributesInfos();

	// TODO : Also check against provided default values

	return true;
}

bool FPCGExMatchmakingElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMatchmakingElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(Matchmaking)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExMatchmaking::FProcessor>>(
			[](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExMatchmaking::FProcessor>* NewBatch)
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

namespace PCGExMatchmaking
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExMatchmaking::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(Matchmaking)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		return true;
	}

	void FProcessor::CompleteWork()
	{
	}

	void FProcessor::Write()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(Matchmaking)

		UPCGMetadata* Metadata = PointDataFacade->GetOut()->Metadata;

		if (Settings->bDoConsumeProcessedAttributes)
		{
			for (const PCGExData::FCacheBase* DataCache : PointDataFacade->Caches)
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
