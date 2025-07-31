// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Actions/PCGExBatchActions.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Graph/States/PCGExClusterStates.h"
#include "Actions/PCGExActionFactoryProvider.h"


#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BatchActions

PCGExData::EIOInit UPCGExBatchActionsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Forward; }

TArray<FPCGPinProperties> UPCGExBatchActionsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExActions::SourceActionsLabel, "Actions nodes.", Normal, {})
	PCGEX_PIN_ANY(PCGExActions::SourceDefaultsLabel, "Default values that match attributes when creating new attributes.", Normal, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(BatchActions)

bool FPCGExBatchActionsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	// Grab all param set attributes

	PCGEX_CONTEXT_AND_SETTINGS(BatchActions)

	if (!PCGExFactories::GetInputFactories(
		Context, PCGExActions::SourceActionsLabel, Context->ActionsFactories,
		{PCGExFactories::EType::Action}, true))
	{
		// No action factories, early exit.
		Context->ActionsFactories.Empty();
		return true;
	}

	FPCGExAttributeGatherDetails DefaultAttributesFilter = Settings->DefaultAttributesFilter;
	DefaultAttributesFilter.Init();
	Context->DefaultAttributes = PCGEx::GatherAttributeInfos(InContext, PCGExActions::SourceDefaultsLabel, DefaultAttributesFilter, true);

	if (!Context->DefaultAttributes) { return false; }

	FString Message = TEXT("An unspecified error occured.");
	bool bIsBatchActionsValid = true;
	PCGEX_MAKE_SHARED(ValidationInfos, PCGEx::FAttributesInfos)
	for (const TObjectPtr<const UPCGExActionFactoryData>& Factory : Context->ActionsFactories)
	{
		if (!Factory->AppendAndValidate(ValidationInfos, Message))
		{
			PCGE_LOG(Error, GraphAndLog, FText::FromString(Message));
			bIsBatchActionsValid = false;
			break;
		}
	}

	if (!bIsBatchActionsValid) { return false; }

	// TODO : Also check against provided default values

	return true;
}

bool FPCGExBatchActionsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBatchActionsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BatchActions)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->ActionsFactories.IsEmpty())
		{
			if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBatchActions::FProcessor>>(
				[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
				[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExBatchActions::FProcessor>>& NewBatch)
				{
				}))
			{
				return Context->CancelExecution(TEXT("Could not find any points to process."));
			}
		}
		else
		{
			// Early exit forward if no action should be processed
			Context->Done();
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExBatchActions
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBatchActions::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;
		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		// Initialize writers with provided default value
		for (FPCGMetadataAttributeBase* AttributeBase : Context->DefaultAttributes->Attributes)
		{
			PCGEx::ExecuteWithRightType(
				AttributeBase->GetTypeId(), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					const FPCGMetadataAttribute<T>* TypedAttribute = static_cast<FPCGMetadataAttribute<T>*>(AttributeBase);
					PointDataFacade->GetWritable<T>(TypedAttribute, PCGExData::EBufferInit::Inherit);
				});
		}

		for (const UPCGExActionFactoryData* Factory : Context->ActionsFactories)
		{
			TSharedPtr<FPCGExActionOperation> Operation = Factory->CreateOperation(Context);
			if (!Operation->PrepareForData(ExecutionContext, PointDataFacade)) { return false; }
			Operations.Add(Operation);
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::BatchActions::ProcessPoints);

		PointDataFacade->Fetch(Scope);

		PCGEX_SCOPE_LOOP(Index)
		{
			for (const TSharedPtr<FPCGExActionOperation>& Op : Operations) { Op->ProcessPoint(Index); }
		}
	}

	void FProcessor::CompleteWork()
	{
		if (Settings->bDoConsumeProcessedAttributes)
		{
			for (const TSharedPtr<PCGExData::IBuffer>& DataCache : PointDataFacade->Buffers)
			{
				if (!DataCache->InAttribute ||
					!Settings->ConsumeProcessedAttributes.Test(DataCache->InAttribute) ||
					PCGEx::IsPCGExAttribute(DataCache->Identifier.Name)) { continue; }

				PointDataFacade->Source->DeleteAttribute(DataCache->InAttribute->Name);
			}
		}

		PointDataFacade->WriteFastest(AsyncManager);
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExBatchActionsContext, UPCGExBatchActionsSettings>::Cleanup();
		Operations.Empty();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
