// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPointsProcessor.h"

#include "PCGPin.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointFilter.h"


#include "Helpers/PCGSettingsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

TArray<FPCGPinProperties> UPCGExPointsProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	if (!IsInputless())
	{
		if (GetMainAcceptMultipleData()) { PCGEX_PIN_POINTS(GetMainInputPin(), "The point data to be processed.", Required, {}) }
		else { PCGEX_PIN_POINT(GetMainInputPin(), "The point data to be processed.", Required, {}) }
	}

	if (SupportsPointFilters())
	{
		if (RequiresPointFilters()) { PCGEX_PIN_PARAMS(GetPointFilterPin(), GetPointFilterTooltip(), Required, {}) }
		else { PCGEX_PIN_PARAMS(GetPointFilterPin(), GetPointFilterTooltip(), Normal, {}) }
	}

	return PinProperties;
}


TArray<FPCGPinProperties> UPCGExPointsProcessorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(GetMainOutputPin(), "The processed points.", Required, {})
	return PinProperties;
}

PCGExData::EIOInit UPCGExPointsProcessorSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::New; }

FPCGExPointsProcessorContext::~FPCGExPointsProcessorContext()
{
	PCGEX_TERMINATE_ASYNC

	for (UPCGExOperation* Op : ProcessorOperations)
	{
		if (OwnedProcessorOperations.Contains(Op))
		{
			ManagedObjects->Destroy(Op);
		}
	}

	if (MainBatch) { MainBatch->Cleanup(); }
	MainBatch.Reset();
}

bool FPCGExPointsProcessorContext::AdvancePointsIO(const bool bCleanupKeys)
{
	if (bCleanupKeys && CurrentIO) { CurrentIO->CleanupKeys(); }

	if (MainPoints->Pairs.IsValidIndex(++CurrentPointIOIndex))
	{
		CurrentIO = MainPoints->Pairs[CurrentPointIOIndex];
		return true;
	}

	CurrentIO = nullptr;
	return false;
}


#pragma endregion

bool FPCGExPointsProcessorContext::ProcessPointsBatch(const PCGEx::AsyncState NextStateId, const bool bIsNextStateAsync)
{
	if (!bBatchProcessingEnabled) { return true; }

	PCGEX_ON_ASYNC_STATE_READY_INTERNAL(PCGExPointsMT::MTState_PointsProcessing)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPointsProcessorContext::BatchProcessing_InitialProcessingDone);
		BatchProcessing_InitialProcessingDone();
		SetAsyncState(PCGExPointsMT::MTState_PointsCompletingWork);
		MainBatch->CompleteWork();
	}

	PCGEX_ON_ASYNC_STATE_READY_INTERNAL(PCGExPointsMT::MTState_PointsCompletingWork)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPointsProcessorContext::BatchProcessing_WorkComplete);
		BatchProcessing_WorkComplete();
		if (MainBatch->bRequiresWriteStep)
		{
			SetAsyncState(PCGExPointsMT::MTState_PointsWriting);
			MainBatch->Write();
			return false;
		}

		bBatchProcessingEnabled = false;
		if (NextStateId == PCGEx::State_Done) { Done(); }
		if (bIsNextStateAsync) { SetAsyncState(NextStateId); }
		else { SetState(NextStateId); }
	}

	PCGEX_ON_ASYNC_STATE_READY_INTERNAL(PCGExPointsMT::MTState_PointsWriting)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPointsProcessorContext::BatchProcessing_WritingDone);
		BatchProcessing_WritingDone();

		bBatchProcessingEnabled = false;
		if (NextStateId == PCGEx::State_Done) { Done(); }
		if (bIsNextStateAsync) { SetAsyncState(NextStateId); }
		else { SetState(NextStateId); }
	}

	return false;
}

TSharedPtr<PCGExMT::FTaskManager> FPCGExPointsProcessorContext::GetAsyncManager()
{
	if (!AsyncManager)
	{
		FWriteScopeLock WriteLock(AsyncLock);
		AsyncManager = MakeShared<PCGExMT::FTaskManager>(this);
		AsyncManager->ForceSync = !bAsyncEnabled;

		PCGEX_SETTINGS_LOCAL(PointsProcessor)
		PCGExMT::SetWorkPriority(Settings->WorkPriority, AsyncManager->WorkPriority);
	}

	return AsyncManager;
}


bool FPCGExPointsProcessorContext::ShouldWaitForAsync()
{
	if (!AsyncManager)
	{
		if (bWaitingForAsyncCompletion) { ResumeExecution(); }
		return false;
	}

	return FPCGExContext::ShouldWaitForAsync();
}

void FPCGExPointsProcessorContext::ResumeExecution()
{
	if (AsyncManager) { AsyncManager->Reset(); }
	FPCGExContext::ResumeExecution();
}

bool FPCGExPointsProcessorContext::IsAsyncWorkComplete()
{
	// Context must be unpaused for this to be called

	if (!bAsyncEnabled) { return true; }
	if (!bWaitingForAsyncCompletion || !AsyncManager) { return true; }

	if (AsyncManager->IsWorkComplete())
	{
		ResumeExecution();
		return true;
	}

	return false;
}

bool FPCGExPointsProcessorElement::PrepareDataInternal(FPCGContext* InContext) const
{
	FPCGExPointsProcessorContext* Context = static_cast<FPCGExPointsProcessorContext*>(InContext);
	check(Context);

	if (!Context->GetInputSettings<UPCGSettings>()->bEnabled)
	{
		Context->bExecutionCancelled = true;
		return true;
	}

	PCGEX_EXECUTION_CHECK_C(Context)

	if (Context->IsState(PCGEx::State_Preparation))
	{
		if (!Boot(Context))
		{
			Context->OutputData.TaggedData.Empty(); // Ensure culling of subsequent nodes if boot fails
			return Context->CancelExecution(TEXT(""));
		}

		Context->RegisterAssetDependencies();
		if (Context->HasAssetRequirements())
		{
			Context->LoadAssets();
			return false;
		}
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGEx::State_LoadingAssetDependencies)
	{
		PostLoadAssetsDependencies(Context);
		PCGEX_EXECUTION_CHECK_C(Context)
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGEx::State_AsyncPreparation)
	{
		PCGEX_EXECUTION_CHECK_C(Context)
	}

	if (!PostBoot(Context))
	{
		return Context->CancelExecution(TEXT("There was a problem during post-data preparation."));
	}

	Context->ReadyForExecution();
	return IPCGElement::PrepareDataInternal(Context);
}

FPCGContext* FPCGExPointsProcessorElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExPointsProcessorContext* Context = new FPCGExPointsProcessorContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

bool FPCGExPointsProcessorElement::IsCacheable(const UPCGSettings* InSettings) const
{
	const UPCGExPointsProcessorSettings* Settings = static_cast<const UPCGExPointsProcessorSettings*>(InSettings);
	return Settings->bCacheResult;
}

void FPCGExPointsProcessorElement::DisabledPassThroughData(FPCGContext* Context) const
{
	const UPCGExPointsProcessorSettings* Settings = Context->GetInputSettings<UPCGExPointsProcessorSettings>();
	check(Settings);

	//Forward main points
	TArray<FPCGTaggedData> MainSources = Context->InputData.GetInputsByPin(Settings->GetMainInputPin());
	for (const FPCGTaggedData& TaggedData : MainSources)
	{
		FPCGTaggedData& TaggedDataCopy = Context->OutputData.TaggedData.Emplace_GetRef();
		TaggedDataCopy.Data = TaggedData.Data;
		TaggedDataCopy.Tags.Append(TaggedData.Tags);
		TaggedDataCopy.Pin = Settings->GetMainOutputPin();
	}
}

FPCGExContext* FPCGExPointsProcessorElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	check(SourceComponent.IsValid());

	InContext->InputData = InputData;
	InContext->SourceComponent = SourceComponent;
	InContext->Node = Node;

	InContext->SetState(PCGEx::State_Preparation);

	const UPCGExPointsProcessorSettings* Settings = InContext->GetInputSettings<UPCGExPointsProcessorSettings>();
	check(Settings);

	InContext->bFlattenOutput = Settings->bFlattenOutput;
	InContext->bAsyncEnabled = Settings->bDoAsyncProcessing;
	InContext->ChunkSize = FMath::Max((Settings->ChunkSize <= 0 ? Settings->GetPreferredChunkSize() : Settings->ChunkSize), 1);

	InContext->bScopedAttributeGet = Settings->bScopedAttributeGet;

	return InContext;
}

bool FPCGExPointsProcessorElement::Boot(FPCGExContext* InContext) const
{
	FPCGExPointsProcessorContext* Context = static_cast<FPCGExPointsProcessorContext*>(InContext);
	PCGEX_SETTINGS(PointsProcessor)

	Context->bDeleteConsumableAttributes = Settings->bDeleteConsumableAttributes;

	if (Context->InputData.GetInputs().IsEmpty() && !Settings->IsInputless()) { return false; } //Get rid of errors and warning when there is no input

	Context->MainPoints = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->MainPoints->OutputPin = Settings->GetMainOutputPin();

	if (Settings->GetMainAcceptMultipleData())
	{
		TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(Settings->GetMainInputPin());
		Context->MainPoints->Initialize(Sources, Settings->GetMainOutputInitMode());
	}
	else
	{
		const TSharedPtr<PCGExData::FPointIO> SingleInput = PCGExData::TryGetSingleInput(Context, Settings->GetMainInputPin(), false);
		if (SingleInput) { Context->MainPoints->AddUnsafe(SingleInput); }
	}

	if (Context->MainPoints->IsEmpty() && !Settings->IsInputless())
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(FText::FromString(TEXT("Missing {0} inputs (either no data or no points)")), FText::FromName(Settings->GetMainInputPin())));
		return false;
	}

	if (Settings->SupportsPointFilters())
	{
		GetInputFactories(Context, Settings->GetPointFilterPin(), Context->FilterFactories, Settings->GetPointFilterTypes(), false);
		if (Settings->RequiresPointFilters() && Context->FilterFactories.IsEmpty())
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(FTEXT("Missing {0}."), FText::FromName(Settings->GetPointFilterPin())));
			return false;
		}
	}

	return true;
}

void FPCGExPointsProcessorElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
}

bool FPCGExPointsProcessorElement::PostBoot(FPCGExContext* InContext) const
{
	return true;
}

#undef LOCTEXT_NAMESPACE
