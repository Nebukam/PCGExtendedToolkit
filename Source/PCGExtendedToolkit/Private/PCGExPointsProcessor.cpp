// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPointsProcessor.h"

#include "PCGPin.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointFilter.h"


#include "Helpers/PCGSettingsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"


#pragma region Loops

namespace PCGEx
{
	TSharedPtr<PCGExData::FPointIO> FAPointLoop::GetPointIO() const { return PointIO ? PointIO : Context->CurrentIO; }

	bool FPointLoop::Advance(const TFunction<void(const TSharedPtr<PCGExData::FPointIO>&)>&& Initialize, const TFunction<void(const int32, const TSharedPtr<PCGExData::FPointIO>&)>&& LoopBody)
	{
		if (CurrentIndex == -1)
		{
			const TSharedPtr<PCGExData::FPointIO> PtIO = GetPointIO();
			Initialize(PtIO);
			NumIterations = PtIO->GetNum();
			CurrentIndex = 0;
		}
		return Advance(std::move(LoopBody));
	}

	bool FPointLoop::Advance(const TFunction<void(const int32, const TSharedPtr<PCGExData::FPointIO>&)>&& LoopBody)
	{
		const TSharedPtr<PCGExData::FPointIO> PtIO = GetPointIO();
		if (CurrentIndex == -1)
		{
			NumIterations = PtIO->GetNum();
			CurrentIndex = 0;
		}
		const int32 ChunkNumIterations = FMath::Min(NumIterations - CurrentIndex, GetCurrentChunkSize());
		if (ChunkNumIterations > 0)
		{
			for (int i = 0; i < ChunkNumIterations; ++i) { LoopBody(CurrentIndex + i, PtIO); }
			CurrentIndex += ChunkNumIterations;
		}
		if (CurrentIndex >= NumIterations)
		{
			CurrentIndex = -1;
			return true;
		}
		return false;
	}

	bool FAsyncPointLoop::Advance(const TFunction<void(const TSharedPtr<PCGExData::FPointIO>&)>&& Initialize, const TFunction<void(const int32, const TSharedPtr<PCGExData::FPointIO>&)>&& LoopBody)
	{
		if (!bAsyncEnabled) { return FPointLoop::Advance(std::move(Initialize), std::move(LoopBody)); }

		const TSharedPtr<PCGExData::FPointIO> PtIO = GetPointIO();
		NumIterations = PtIO->GetNum();
		return FPCGAsync::AsyncProcessingOneToOneEx(
			&(Context->AsyncState), NumIterations, [&]()
			{
				Initialize(PtIO);
			}, [&](const int32 ReadIndex, const int32 WriteIndex)
			{
				LoopBody(ReadIndex, PtIO);
				return true;
			}, true, ChunkSize);
	}

	bool FAsyncPointLoop::Advance(const TFunction<void(const int32, const TSharedPtr<PCGExData::FPointIO>&)>&& LoopBody)
	{
		if (!bAsyncEnabled) { return FPointLoop::Advance(std::move(LoopBody)); }

		const TSharedPtr<PCGExData::FPointIO> PtIO = GetPointIO();
		NumIterations = PtIO->GetNum();
		return FPCGAsync::AsyncProcessingOneToOneEx(
			&(Context->AsyncState), NumIterations, []()
			{
			}, [&](const int32 ReadIndex, const int32 WriteIndex)
			{
				LoopBody(ReadIndex, PtIO);
				return true;
			}, true, ChunkSize);
	}
}
#pragma endregion

#pragma region UPCGSettings interface

TArray<FPCGPinProperties> UPCGExPointsProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	if (GetMainAcceptMultipleData()) { PCGEX_PIN_POINTS(GetMainInputLabel(), "The point data to be processed.", Required, {}) }
	else { PCGEX_PIN_POINT(GetMainInputLabel(), "The point data to be processed.", Required, {}) }

	if (SupportsPointFilters())
	{
		if (RequiresPointFilters()) { PCGEX_PIN_PARAMS(GetPointFilterLabel(), GetPointFilterTooltip(), Required, {}) }
		else { PCGEX_PIN_PARAMS(GetPointFilterLabel(), GetPointFilterTooltip(), Normal, {}) }
	}

	return PinProperties;
}


TArray<FPCGPinProperties> UPCGExPointsProcessorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(GetMainOutputLabel(), "The processed points.", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExPointsProcessorSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FPCGExPointsProcessorContext::~FPCGExPointsProcessorContext()
{	
	PCGEX_TERMINATE_ASYNC

	
	for (UPCGExOperation* Operation : ProcessorOperations)
	{
		if (OwnedProcessorOperations.Contains(Operation)) { PCGEX_DELETE_OPERATION(Operation) }
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

void FPCGExPointsProcessorContext::SetAsyncState(const PCGExMT::AsyncState WaitState)
{
	if (!bDoAsyncProcessing)
	{
		SetState(WaitState);
		return;
	}

	bWaitingForAsyncCompletion = true;
	SetState(WaitState);
}


void FPCGExPointsProcessorContext::SetState(const PCGExMT::AsyncState StateId)
{
	if (CurrentState == StateId) { return; }
	CurrentState = StateId;
}

void FPCGExPointsProcessorContext::Done()
{
	bUseLock = false;
	SetState(PCGExMT::State_Done);
}

bool FPCGExPointsProcessorContext::TryComplete(const bool bForce)
{
	if (!bForce && !IsDone()) { return false; }
	OnComplete();
	return true;
}

#pragma endregion

bool FPCGExPointsProcessorContext::ProcessPointsBatch(const PCGExMT::AsyncState NextStateId, const bool bIsNextStateAsync)
{
	if (!bBatchProcessingEnabled) { return true; }

	if (IsState(PCGExPointsMT::MTState_PointsProcessing))
	{
		PCGEX_ASYNC_WAIT_INTERNAL

		BatchProcessing_InitialProcessingDone();
		MainBatch->CompleteWork();
		SetAsyncState(PCGExPointsMT::MTState_PointsCompletingWork);
	}

	if (IsState(PCGExPointsMT::MTState_PointsCompletingWork))
	{
		PCGEX_ASYNC_WAIT_INTERNAL

		BatchProcessing_WorkComplete();

		if (MainBatch->bRequiresWriteStep)
		{
			MainBatch->Write();
			SetAsyncState(PCGExPointsMT::MTState_PointsWriting);
			return false;
		}

		bBatchProcessingEnabled = false;
		if (NextStateId == PCGExMT::State_Done) { Done(); }
		if (bIsNextStateAsync) { SetAsyncState(NextStateId); }
		else { SetState(NextStateId); }
	}

	if (IsState(PCGExPointsMT::MTState_PointsWriting))
	{
		PCGEX_ASYNC_WAIT_INTERNAL

		BatchProcessing_WritingDone();

		bBatchProcessingEnabled = false;
		if (NextStateId == PCGExMT::State_Done) { Done(); }
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
		AsyncManager->ForceSync = !bDoAsyncProcessing;

		PCGEX_SETTINGS_LOCAL(PointsProcessor)
		PCGExMT::SetWorkPriority(Settings->WorkPriority, AsyncManager->WorkPriority);
	}

	return AsyncManager;
}

void FPCGExPointsProcessorContext::ResumeExecution()
{
	if (AsyncManager) { AsyncManager->Reset(); }

	StopAsyncWork();
	bWaitingForAsyncCompletion = false;
}

bool FPCGExPointsProcessorContext::IsAsyncWorkComplete()
{
	// Context must be unpaused for this to be called

	if (!bDoAsyncProcessing) { return true; }
	if (!bWaitingForAsyncCompletion || !AsyncManager) { return true; }

	if (AsyncManager->IsWorkComplete())
	{
		ResumeExecution();
		return true;
	}

	return false;
}

bool FPCGExPointsProcessorElement::PrepareDataInternal(FPCGContext* Context) const
{
	FPCGExContext* Ctx = static_cast<FPCGExContext*>(Context);
	if (Ctx)
	{
		if (!Ctx->WasAssetLoadRequested())
		{
			Ctx->RegisterAssetDependencies();
			if (Ctx->HasAssetRequirements())
			{
				Ctx->LoadAssets();
				return false;
			}
		}
	}

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
	TArray<FPCGTaggedData> MainSources = Context->InputData.GetInputsByPin(Settings->GetMainInputLabel());
	for (const FPCGTaggedData& TaggedData : MainSources)
	{
		FPCGTaggedData& TaggedDataCopy = Context->OutputData.TaggedData.Emplace_GetRef();
		TaggedDataCopy.Data = TaggedData.Data;
		TaggedDataCopy.Tags.Append(TaggedData.Tags);
		TaggedDataCopy.Pin = Settings->GetMainOutputLabel();
	}
}

FPCGContext* FPCGExPointsProcessorElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	InContext->InputData = InputData;
	InContext->SourceComponent = SourceComponent;
	InContext->Node = Node;

	check(SourceComponent.IsValid());
	InContext->World = SourceComponent->GetWorld();

	const UPCGExPointsProcessorSettings* Settings = InContext->GetInputSettings<UPCGExPointsProcessorSettings>();
	check(Settings);

	InContext->bFlattenOutput = Settings->bFlattenOutput;

	InContext->SetState(PCGExMT::State_Setup);
	InContext->bDoAsyncProcessing = Settings->bDoAsyncProcessing;
	InContext->ChunkSize = FMath::Max((Settings->ChunkSize <= 0 ? Settings->GetPreferredChunkSize() : Settings->ChunkSize), 1);

	InContext->AsyncLoop = InContext->MakeLoop<PCGExMT::FAsyncParallelLoop>();

	InContext->MainPoints = MakeShared<PCGExData::FPointIOCollection>(InContext);
	InContext->MainPoints->DefaultOutputLabel = Settings->GetMainOutputLabel();

	if (!Settings->bEnabled) { return InContext; }

	if (Settings->GetMainAcceptMultipleData())
	{
		TArray<FPCGTaggedData> Sources = InContext->InputData.GetInputsByPin(Settings->GetMainInputLabel());
		InContext->MainPoints->Initialize(Sources, Settings->GetMainOutputInitMode());
	}
	else
	{
		TArray<FPCGTaggedData> Sources = InContext->InputData.GetInputsByPin(Settings->GetMainInputLabel());
		const UPCGPointData* InData = nullptr;
		const FPCGTaggedData* Source = nullptr;
		int32 SrcIndex = -1;

		while (!InData && Sources.IsValidIndex(++SrcIndex))
		{
			InData = PCGExData::GetMutablePointData(InContext, Sources[SrcIndex]);
			if (InData && !InData->GetPoints().IsEmpty()) { Source = &Sources[SrcIndex]; }
			else { InData = nullptr; }
		}

		if (InData)
		{
			InContext->MainPoints->Emplace_GetRef(InData, Settings->GetMainOutputInitMode(), &Source->Tags);
		}
	}

	return InContext;
}

bool FPCGExPointsProcessorElement::Boot(FPCGExContext* InContext) const
{
	FPCGExPointsProcessorContext* Context = static_cast<FPCGExPointsProcessorContext*>(InContext);
	PCGEX_SETTINGS(PointsProcessor)

	Context->bScopedAttributeGet = Settings->bScopedAttributeGet;

	if (Context->bAssetLoadError)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("An error occured while loading asset dependencies."));
		return false;
	}

	if (Context->InputData.GetInputs().IsEmpty()) { return false; } //Get rid of errors and warning when there is no input

	if (Context->MainPoints->IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(FText::FromString(TEXT("Missing {0} inputs")), FText::FromName(Settings->GetMainInputLabel())));
		return false;
	}

	if (Settings->SupportsPointFilters())
	{
		GetInputFactories(
			InContext, Settings->GetPointFilterLabel(), Context->FilterFactories,
			Settings->GetPointFilterTypes(), false);
		if (Settings->RequiresPointFilters() && Context->FilterFactories.IsEmpty())
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(FTEXT("Missing {0}."), FText::FromName(Settings->GetPointFilterLabel())));
			return false;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
