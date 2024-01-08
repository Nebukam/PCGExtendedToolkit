// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPointsProcessor.h"

#include "PCGPin.h"
#include "Data/PCGExData.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"


#pragma region Loops

PCGExData::FPointIO& PCGEx::FAPointLoop::GetPointIO() const { return PointIO ? *PointIO : *Context->CurrentIO; }

bool PCGEx::FPointLoop::Advance(const TFunction<void(PCGExData::FPointIO&)>&& Initialize, const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody)
{
	if (CurrentIndex == -1)
	{
		PCGExData::FPointIO& PtIO = GetPointIO();
		Initialize(PtIO);
		NumIterations = PtIO.GetNum();
		CurrentIndex = 0;
	}
	return Advance(std::move(LoopBody));
}

bool PCGEx::FPointLoop::Advance(const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody)
{
	const PCGExData::FPointIO& PtIO = GetPointIO();
	if (CurrentIndex == -1)
	{
		NumIterations = PtIO.GetNum();
		CurrentIndex = 0;
	}
	const int32 ChunkNumIterations = FMath::Min(NumIterations - CurrentIndex, GetCurrentChunkSize());
	if (ChunkNumIterations > 0)
	{
		for (int i = 0; i < ChunkNumIterations; i++) { LoopBody(CurrentIndex + i, PtIO); }
		CurrentIndex += ChunkNumIterations;
	}
	if (CurrentIndex >= NumIterations)
	{
		CurrentIndex = -1;
		return true;
	}
	return false;
}

void PCGEx::FBulkPointLoop::Init()
{
	const int32 NumLoops = Context->MainPoints->Num();
	if (CurrentIndex == -1)
	{
		CurrentIndex = 0;
		SubLoops.Reset(NumLoops);
		for (int i = 0; i < NumLoops; i++)
		{
			FPointLoop& SubLoop = SubLoops.Emplace_GetRef();
			SubLoop.Context = Context;
			SubLoop.ChunkSize = ChunkSize;
			SubLoop.PointIO = Context->MainPoints->Pairs[i];
		}
	}
}

bool PCGEx::FBulkPointLoop::Advance(const TFunction<void(PCGExData::FPointIO&)>&& Initialize, const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody)
{
	Init();
	for (int i = 0; i < SubLoops.Num(); i++)
	{
		if (SubLoops[i].Advance(std::move(Initialize), std::move(LoopBody)))
		{
			SubLoops.RemoveAt(i);
			i--;
		}
	}

	if (SubLoops.IsEmpty())
	{
		CurrentIndex = -1;
		return true;
	}

	return false;
}

bool PCGEx::FBulkPointLoop::Advance(const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody)
{
	Init();
	for (int i = 0; i < SubLoops.Num(); i++)
	{
		if (SubLoops[i].Advance(std::move(LoopBody)))
		{
			SubLoops.RemoveAt(i);
			i--;
		}
	}

	if (SubLoops.IsEmpty())
	{
		CurrentIndex = -1;
		return true;
	}

	return false;
}

bool PCGEx::FAsyncPointLoop::Advance(const TFunction<void(PCGExData::FPointIO&)>&& Initialize, const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody)
{
	if (!bAsyncEnabled) { return FPointLoop::Advance(std::move(Initialize), std::move(LoopBody)); }

	PCGExData::FPointIO& PtIO = GetPointIO();
	NumIterations = PtIO.GetNum();
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

bool PCGEx::FAsyncPointLoop::Advance(const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody)
{
	if (!bAsyncEnabled) { return FPointLoop::Advance(std::move(LoopBody)); }

	const PCGExData::FPointIO& PtIO = GetPointIO();
	NumIterations = PtIO.GetNum();
	return FPCGAsync::AsyncProcessingOneToOneEx(
		&(Context->AsyncState), NumIterations, []()
		{
		}, [&](const int32 ReadIndex, const int32 WriteIndex)
		{
			LoopBody(ReadIndex, PtIO);
			return true;
		}, true, ChunkSize);
}

void PCGEx::FBulkAsyncPointLoop::Init()
{
	const int32 NumLoops = Context->MainPoints->Num();
	if (CurrentIndex == -1)
	{
		CurrentIndex = 0;
		SubLoops.Reset(NumLoops);
		for (int i = 0; i < NumLoops; i++)
		{
			FAsyncPointLoop& SubLoop = SubLoops.Emplace_GetRef();
			SubLoop.Context = Context;
			SubLoop.bAsyncEnabled = Context->bDoAsyncProcessing;
			SubLoop.ChunkSize = ChunkSize;
			SubLoop.PointIO = Context->MainPoints->Pairs[i];
		}
	}
}

bool PCGEx::FBulkAsyncPointLoop::Advance(const TFunction<void(PCGExData::FPointIO&)>&& Initialize, const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody)
{
	Init();
	for (int i = 0; i < SubLoops.Num(); i++)
	{
		if (SubLoops[i].Advance(std::move(Initialize), std::move(LoopBody)))
		{
			SubLoops.RemoveAt(i);
			i--;
		}
	}

	if (SubLoops.IsEmpty())
	{
		CurrentIndex = -1;
		return true;
	}

	return false;
}

bool PCGEx::FBulkAsyncPointLoop::Advance(const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody)
{
	Init();
	for (int i = 0; i < SubLoops.Num(); i++)
	{
		if (SubLoops[i].Advance(std::move(LoopBody)))
		{
			SubLoops.RemoveAt(i);
			i--;
		}
	}

	if (SubLoops.IsEmpty())
	{
		CurrentIndex = -1;
		return true;
	}

	return false;
}

#pragma endregion


#pragma region UPCGSettings interface

UPCGExPointsProcessorSettings::UPCGExPointsProcessorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
void UPCGExPointsProcessorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

TArray<FPCGPinProperties> UPCGExPointsProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	bool bAcceptMultiple = GetMainAcceptMultipleData();
	FPCGPinProperties& PinPropertySource = PinProperties.Emplace_GetRef(GetMainInputLabel(), EPCGDataType::Point, bAcceptMultiple, bAcceptMultiple);

#if WITH_EDITOR
	PinPropertySource.Tooltip = FTEXT("The point data to be processed.");
#endif

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPointsProcessorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPointsOutput = PinProperties.Emplace_GetRef(GetMainOutputLabel(), EPCGDataType::Point);

#if WITH_EDITOR
	PinPointsOutput.Tooltip = FTEXT("The processed points.");
#endif

	return PinProperties;
}

FName UPCGExPointsProcessorSettings::GetMainOutputLabel() const { return PCGEx::OutputPointsLabel; }
FName UPCGExPointsProcessorSettings::GetMainInputLabel() const { return PCGEx::SourcePointsLabel; }

bool UPCGExPointsProcessorSettings::GetMainAcceptMultipleData() const { return true; }

PCGExData::EInit UPCGExPointsProcessorSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

int32 UPCGExPointsProcessorSettings::GetPreferredChunkSize() const { return 256; }

FPCGExPointsProcessorContext::~FPCGExPointsProcessorContext()
{
	PCGEX_TERMINATE_ASYNC

	CleanupOperations();
	for (UPCGExOperation* Operation : OwnedProcessorOperations) { Operation->ConditionalBeginDestroy(); }

	ProcessorOperations.Empty();
	OwnedProcessorOperations.Empty();

	PCGEX_DELETE(MainPoints)

	CurrentIO = nullptr;
	World = nullptr;
}

bool FPCGExPointsProcessorContext::AdvancePointsIO()
{
	if (CurrentIO) { CurrentIO->Cleanup(); }

	if (MainPoints->Pairs.IsValidIndex(++CurrentPointsIndex))
	{
		CurrentIO = MainPoints->Pairs[CurrentPointsIndex];
		return true;
	}
	CurrentIO = nullptr;
	return false;
}

void FPCGExPointsProcessorContext::Done()
{
	SetState(PCGExMT::State_Done);
}

void FPCGExPointsProcessorContext::SetState(const PCGExMT::AsyncState OperationId, const bool bResetAsyncWork)
{
	if (bResetAsyncWork) { ResetAsyncWork(); }
	CurrentState = OperationId;
}

void FPCGExPointsProcessorContext::Reset() { CurrentState = PCGExMT::State_Setup; }

#pragma endregion

bool FPCGExPointsProcessorContext::BulkProcessMainPoints(TFunction<void(PCGExData::FPointIO&)>&& Initialize, TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody)
{
	return BulkAsyncPointLoop.Advance(std::move(Initialize), std::move(LoopBody));
}

bool FPCGExPointsProcessorContext::ProcessCurrentPoints(TFunction<void(PCGExData::FPointIO&)>&& Initialize, TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody, const bool bForceSync)
{
	return bForceSync ? ChunkedPointLoop.Advance(std::move(Initialize), std::move(LoopBody)) : AsyncPointLoop.Advance(std::move(Initialize), std::move(LoopBody));
}

bool FPCGExPointsProcessorContext::ProcessCurrentPoints(TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody, const bool bForceSync)
{
	return bForceSync ? ChunkedPointLoop.Advance(std::move(LoopBody)) : AsyncPointLoop.Advance(std::move(LoopBody));
}

void FPCGExPointsProcessorContext::Output(FPCGTaggedData& OutTaggedData, UPCGData* OutData, const FName OutputLabel)
{
	FWriteScopeLock WriteLock(ContextLock);
	FPCGTaggedData& OutputRef = OutputData.TaggedData.Emplace_GetRef(OutTaggedData);
	OutputRef.Data = OutData;
	OutputRef.Pin = OutputLabel;
}

void FPCGExPointsProcessorContext::Output(UPCGData* OutData, const FName OutputLabel)
{
	FWriteScopeLock WriteLock(ContextLock);
	FPCGTaggedData& OutputRef = OutputData.TaggedData.Emplace_GetRef();
	OutputRef.Data = OutData;
	OutputRef.Pin = OutputLabel;
}

void FPCGExPointsProcessorContext::Output(PCGExData::FPointIO& PointIO)
{
	UPCGPointData* OutData = PointIO.GetOut();
	if (!OutData || OutData->GetPoints().Num() == 0) { return; }

	FWriteScopeLock WriteLock(ContextLock);

	FPCGTaggedData& OutputRef = OutputData.TaggedData.Emplace_GetRef();
	OutputRef.Data = OutData;
	OutputRef.Pin = PointIO.DefaultOutputLabel;

	PointIO.Cleanup();
}

FPCGExAsyncManager* FPCGExPointsProcessorContext::GetAsyncManager()
{
	if (!AsyncManager)
	{
		FWriteScopeLock WriteLock(ContextLock);
		AsyncManager = new FPCGExAsyncManager();
		AsyncManager->bForceSync = !bDoAsyncProcessing;
		AsyncManager->Context = this;
	}
	return AsyncManager;
}

void FPCGExPointsProcessorContext::CleanupOperations()
{
	for (UPCGExOperation* Operation : ProcessorOperations) { Operation->Cleanup(); }
}

void FPCGExPointsProcessorContext::ResetAsyncWork() { if (AsyncManager) { AsyncManager->Reset(); } }
bool FPCGExPointsProcessorContext::IsAsyncWorkComplete() { return AsyncManager ? AsyncManager->IsAsyncWorkComplete() : true; }

FPCGContext* FPCGExPointsProcessorElementBase::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExPointsProcessorContext* Context = new FPCGExPointsProcessorContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

FPCGContext* FPCGExPointsProcessorElementBase::InitializeContext(
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

	InContext->SetState(PCGExMT::State_Setup);
	InContext->bDoAsyncProcessing = Settings->bDoAsyncProcessing;
	InContext->ChunkSize = FMath::Max((Settings->ChunkSize <= 0 ? Settings->GetPreferredChunkSize() : Settings->ChunkSize), 1);

	InContext->AsyncLoop = InContext->MakeLoop<PCGExMT::FAsyncParallelLoop>();

	InContext->ChunkedPointLoop = InContext->MakeLoop<PCGEx::FPointLoop>();
	InContext->AsyncPointLoop = InContext->MakeLoop<PCGEx::FAsyncPointLoop>();
	InContext->BulkAsyncPointLoop = InContext->MakeLoop<PCGEx::FBulkAsyncPointLoop>();

	InContext->MainPoints = new PCGExData::FPointIOGroup();
	InContext->MainPoints->DefaultOutputLabel = Settings->GetMainOutputLabel();

	if (Settings->GetMainAcceptMultipleData())
	{
		TArray<FPCGTaggedData> Sources = InContext->InputData.GetInputsByPin(Settings->GetMainInputLabel());
		InContext->MainPoints->Initialize(InContext, Sources, Settings->GetMainOutputInitMode());
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

		if (InData) { InContext->MainPoints->Emplace_GetRef(*Source, InData, Settings->GetMainOutputInitMode()); }
	}

	return InContext;
}

bool FPCGExPointsProcessorElementBase::Boot(FPCGContext* InContext) const
{
	const FPCGExPointsProcessorContext* Context = static_cast<FPCGExPointsProcessorContext*>(InContext);

	if (Context->InputData.GetInputs().IsEmpty()) { return false; } //Get rid of errors and warning when there is no input

	if (Context->MainPoints->IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Input Points."));
		return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
