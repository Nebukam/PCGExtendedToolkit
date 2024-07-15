// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExConditionalActions.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Graph/PCGExCluster.h"
#include "Graph/States/PCGExClusterStates.h"
#include "Misc/ConditionalActions/PCGExConditionalActionFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE ConditionalActions

int32 UPCGExConditionalActionsSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_M; }

PCGExData::EInit UPCGExConditionalActionsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

TArray<FPCGPinProperties> UPCGExConditionalActionsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExConditionalActions::SourceConditionalActionsLabel, "Conditional Actions nodes.", Required, {})
	PCGEX_PIN_ANY(PCGExConditionalActions::SourceDefaultsLabel, "Default values that match attributes when creating new attributes through matchmaking.", Normal, {})
	return PinProperties;
}

FPCGExConditionalActionsContext::~FPCGExConditionalActionsContext()
{
	PCGEX_TERMINATE_ASYNC

	ConditionalActionsFactories.Empty();

	PCGEX_DELETE(DefaultAttributes)
}

PCGEX_INITIALIZE_ELEMENT(ConditionalActions)

bool FPCGExConditionalActionsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	// Grab all param set attributes

	PCGEX_CONTEXT_AND_SETTINGS(ConditionalActions)

	if (!PCGExFactories::GetInputFactories(
		Context, PCGExConditionalActions::SourceConditionalActionsLabel, Context->ConditionalActionsFactories,
		{PCGExFactories::EType::ConditionalActions}, true))
	{
		return false;
	}

	FPCGExAttributeGatherDetails DefaultAttributesFilter = Settings->DefaultAttributesFilter;
	DefaultAttributesFilter.Init();
	Context->DefaultAttributes = PCGEx::GatherAttributeInfos(InContext, PCGExConditionalActions::SourceDefaultsLabel, DefaultAttributesFilter, true);

	if (!Context->DefaultAttributes) { return false; }

	FString Message = TEXT("An unspecified error occured.");
	bool bIsConditionalActionsValid = true;
	PCGEx::FAttributesInfos* ValidationInfos = new PCGEx::FAttributesInfos();
	for (UPCGExConditionalActionFactoryBase* Factory : Context->ConditionalActionsFactories)
	{
		if (!Factory->AppendAndValidate(ValidationInfos, Message))
		{
			PCGE_LOG(Error, GraphAndLog, FText::FromString(Message));
			bIsConditionalActionsValid = false;
			break;
		}
	}

	PCGEX_DELETE(ValidationInfos)

	if (!bIsConditionalActionsValid) { return false; }


	// TODO : Also check against provided default values

	return true;
}

bool FPCGExConditionalActionsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExConditionalActionsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ConditionalActions)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExConditionalActions::FProcessor>>(
			[](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExConditionalActions::FProcessor>* NewBatch)
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

namespace PCGExConditionalActions
{
	FProcessor::~FProcessor()
	{
		for (UPCGExConditionalActionOperation* Op : Operations) { PCGEX_DELETE_UOBJECT(Op) }
		Operations.Empty();
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExConditionalActions::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ConditionalActions)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		PointDataFacade->bSupportsDynamic = true;
		
		// Initialize writers with provided default value
		for (FPCGMetadataAttributeBase* AttributeBase : TypedContext->DefaultAttributes->Attributes)
		{
			PCGMetadataAttribute::CallbackWithRightType(
				static_cast<uint16>(AttributeBase->GetTypeId()), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					const FPCGMetadataAttribute<T>* TypedAttribute = static_cast<FPCGMetadataAttribute<T>*>(AttributeBase);
					PointDataFacade->GetWriter<T>(TypedAttribute, false);
				});
		}

		for (const UPCGExConditionalActionFactoryBase* Factory : TypedContext->ConditionalActionsFactories)
		{
			UPCGExConditionalActionOperation* Operation = Factory->CreateOperation();
			if (!Operation->PrepareForData(Context, PointDataFacade)) { return false; }
			Operations.Add(Operation);
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		for (UPCGExConditionalActionOperation* Op : Operations) { Op->ProcessPoint(Index, Point); }
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ConditionalActions)

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

		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
