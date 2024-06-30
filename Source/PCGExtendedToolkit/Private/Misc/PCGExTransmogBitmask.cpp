// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExTransmogBitmask.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Graph/PCGExCluster.h"
#include "Graph/States/PCGExClusterStates.h"
#include "Misc/Transmogs/PCGExBitmaskTransmog.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE TransmogBitmask

int32 UPCGExTransmogBitmaskSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_M; }

PCGExData::EInit UPCGExTransmogBitmaskSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

TArray<FPCGPinProperties> UPCGExTransmogBitmaskSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExBitmaskTransmog::SourceTransmogsLabel, "Node states.", Required, {})
	PCGEX_PIN_ANY(PCGExBitmaskTransmog::SourceDefaultsLabel, "Default values that match transmuted attributes when creating new attributes.", Normal, {})
	return PinProperties;
}

FPCGExTransmogBitmaskContext::~FPCGExTransmogBitmaskContext()
{
	PCGEX_TERMINATE_ASYNC

	TransmogsFactories.Empty();

	PCGEX_DELETE(DefaultAttributes)
}

PCGEX_INITIALIZE_ELEMENT(TransmogBitmask)

bool FPCGExTransmogBitmaskElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	// Grab all param set attributes

	PCGEX_CONTEXT_AND_SETTINGS(TransmogBitmask)

	if (!PCGExFactories::GetInputFactories(
		Context, PCGExBitmaskTransmog::SourceTransmogsLabel,
		Context->TransmogsFactories, {PCGExFactories::EType::BitmaskTransmog}, true))
	{
		return false;
	}

	FString Message = TEXT("An unspecified error occured.");
	bool bIsTransmogValid = true;
	PCGEx::FAttributesInfos* ValidationInfos = new PCGEx::FAttributesInfos();
	for (UPCGExBitmaskTransmogFactoryBase* Factory : Context->TransmogsFactories)
	{
		if (!Factory->AppendAndValidate(ValidationInfos, Message))
		{
			PCGE_LOG(Error, GraphAndLog, FText::FromString(Message));
			bIsTransmogValid = false;
			break;
		}
	}

	PCGEX_DELETE(ValidationInfos)

	if (!bIsTransmogValid) { return false; }

	Context->DefaultAttributes = new PCGEx::FAttributesInfos();

	// TODO : Also check against provided default values

	return true;
}

bool FPCGExTransmogBitmaskElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExTransmogBitmaskElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(TransmogBitmask)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExTransmogBitmask::FProcessor>>(
			[](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExTransmogBitmask::FProcessor>* NewBatch)
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

namespace PCGExTransmogBitmask
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(TransmogBitmask)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		return true;
	}

	void FProcessor::CompleteWork()
	{
	}

	void FProcessor::Write()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(TransmogBitmask)
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
