// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExConditionalActions.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Graph/States/PCGExClusterStates.h"
#include "Misc/ConditionalActions/PCGExConditionalActionFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE ConditionalActions

int32 UPCGExConditionalActionsSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_M; }

PCGExData::EIOInit UPCGExConditionalActionsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

TArray<FPCGPinProperties> UPCGExConditionalActionsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExConditionalActions::SourceConditionalActionsLabel, "Conditional Actions nodes.", Required, {})
	PCGEX_PIN_ANY(PCGExConditionalActions::SourceDefaultsLabel, "Default values that match attributes when creating new attributes through matchmaking.", Normal, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(ConditionalActions)

bool FPCGExConditionalActionsElement::Boot(FPCGExContext* InContext) const
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
	const TSharedPtr<PCGEx::FAttributesInfos> ValidationInfos = MakeShared<PCGEx::FAttributesInfos>();
	for (const TObjectPtr<const UPCGExConditionalActionFactoryBase>& Factory : Context->ConditionalActionsFactories)
	{
		if (!Factory->AppendAndValidate(ValidationInfos.Get(), Message))
		{
			PCGE_LOG(Error, GraphAndLog, FText::FromString(Message));
			bIsConditionalActionsValid = false;
			break;
		}
	}

	if (!bIsConditionalActionsValid) { return false; }

	// TODO : Also check against provided default values

	return true;
}

bool FPCGExConditionalActionsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExConditionalActionsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ConditionalActions)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExConditionalActions::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExConditionalActions::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExConditionalActions
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExConditionalActions::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		// Initialize writers with provided default value
		for (FPCGMetadataAttributeBase* AttributeBase : Context->DefaultAttributes->Attributes)
		{
			PCGMetadataAttribute::CallbackWithRightType(
				static_cast<uint16>(AttributeBase->GetTypeId()), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					const FPCGMetadataAttribute<T>* TypedAttribute = static_cast<FPCGMetadataAttribute<T>*>(AttributeBase);
					PointDataFacade->GetWritable<T>(TypedAttribute, PCGExData::EBufferInit::Inherit);
				});
		}

		for (const UPCGExConditionalActionFactoryBase* Factory : Context->ConditionalActionsFactories)
		{
			UPCGExConditionalActionOperation* Operation = Factory->CreateOperation(Context);
			if (!Operation->PrepareForData(ExecutionContext, PointDataFacade)) { return false; }
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
		if (Settings->bDoConsumeProcessedAttributes)
		{
			for (const TSharedPtr<PCGExData::FBufferBase>& DataCache : PointDataFacade->Buffers)
			{
				if (!DataCache->InAttribute ||
					!Settings->ConsumeProcessedAttributes.Test(DataCache->InAttribute) ||
					PCGEx::IsPCGExAttribute(DataCache->FullName)) { continue; }

				PointDataFacade->Source->DeleteAttribute(DataCache->InAttribute->Name);
			}
		}

		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
