// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPointsProcessor.h"

#include "PCGPin.h"

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
	PCGExData::FPointIO& PtIO = GetPointIO();
	if (CurrentIndex == -1)
	{
		NumIterations = PtIO.GetNum();
		CurrentIndex = 0;
	}
	const int32 ChunkNumIterations = GetCurrentChunkSize();
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
			SubLoop.PointIO = &Context->MainPoints->Pairs[i];
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
		}, [&](int32 ReadIndex, int32 WriteIndex)
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
		}, [&](int32 ReadIndex, int32 WriteIndex)
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
			SubLoop.PointIO = &Context->MainPoints->Pairs[i];
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

void UPCGExPointsProcessorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

TArray<FPCGPinProperties> UPCGExPointsProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	bool bAcceptMultiple = GetMainPointsInputAcceptMultipleData();
	FPCGPinProperties& PinPropertySource = PinProperties.Emplace_GetRef(GetMainPointsInputLabel(), EPCGDataType::Point, bAcceptMultiple, bAcceptMultiple);

#if WITH_EDITOR
	PinPropertySource.Tooltip = LOCTEXT("PCGExSourcePointsPinTooltip", "The point data to be processed.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPointsProcessorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPointsOutput = PinProperties.Emplace_GetRef(GetMainPointsOutputLabel(), EPCGDataType::Point);

#if WITH_EDITOR
	PinPointsOutput.Tooltip = LOCTEXT("PCGExOutputPointsPinTooltip", "The processed points.");
#endif // WITH_EDITOR

	return PinProperties;
}

FName UPCGExPointsProcessorSettings::GetMainPointsOutputLabel() const { return PCGEx::OutputPointsLabel; }
FName UPCGExPointsProcessorSettings::GetMainPointsInputLabel() const { return PCGEx::SourcePointsLabel; }

bool UPCGExPointsProcessorSettings::GetMainPointsInputAcceptMultipleData() const { return true; }

PCGExData::EInit UPCGExPointsProcessorSettings::GetPointOutputInitMode() const { return PCGExData::EInit::NewOutput; }

int32 UPCGExPointsProcessorSettings::GetPreferredChunkSize() const { return 256; }

FPCGExPointsProcessorContext::~FPCGExPointsProcessorContext()
{
	PCGEX_DELETE(AsyncManager)
	PCGEX_DELETE(MainPoints)
	CurrentIO = nullptr;
	World = nullptr;
}

bool FPCGExPointsProcessorContext::AdvancePointsIO()
{
	CurrentPointsIndex++;
	if (MainPoints->Pairs.IsValidIndex(CurrentPointsIndex))
	{
		CurrentIO = &MainPoints->Pairs[CurrentPointsIndex];
		return true;
	}
	CurrentIO = nullptr;
	return false;
}

void FPCGExPointsProcessorContext::Done()
{
	SetState(PCGExMT::State_Done);
}

void FPCGExPointsProcessorContext::SetState(PCGExMT::AsyncState OperationId, bool bResetAsyncWork)
{
	if (bResetAsyncWork) { ResetAsyncWork(); }
	PCGExMT::AsyncState PreviousOperation = CurrentState;
	CurrentState = OperationId;
}

void FPCGExPointsProcessorContext::Reset() { CurrentState = PCGExMT::State_Setup; }

bool FPCGExPointsProcessorContext::ValidatePointDataInput(UPCGPointData* PointData) { return true; }

void FPCGExPointsProcessorContext::PostInitPointDataInput(const PCGExData::FPointIO& PointData)
{
}

#pragma endregion

bool FPCGExPointsProcessorContext::BulkProcessMainPoints(TFunction<void(PCGExData::FPointIO&)>&& Initialize, TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody)
{
	return BulkAsyncPointLoop.Advance(std::move(Initialize), std::move(LoopBody));
}

bool FPCGExPointsProcessorContext::ProcessCurrentPoints(TFunction<void(PCGExData::FPointIO&)>&& Initialize, TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody, bool bForceSync)
{
	return bForceSync ? ChunkedPointLoop.Advance(std::move(Initialize), std::move(LoopBody)) : AsyncPointLoop.Advance(std::move(Initialize), std::move(LoopBody));
}

bool FPCGExPointsProcessorContext::ProcessCurrentPoints(TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody, bool bForceSync)
{
	return bForceSync ? ChunkedPointLoop.Advance(std::move(LoopBody)) : AsyncPointLoop.Advance(std::move(LoopBody));
}

FPCGExAsyncManager* FPCGExPointsProcessorContext::GetAsyncManager()
{
	if (!AsyncManager)
	{
		FWriteScopeLock WriteLock(ContextLock);
		AsyncManager = new FPCGExAsyncManager();
		AsyncManager->Context = this;
	}
	return AsyncManager;
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

bool FPCGExPointsProcessorElementBase::Validate(FPCGContext* InContext) const
{
	const FPCGExPointsProcessorContext* Context = static_cast<FPCGExPointsProcessorContext*>(InContext);

	if (Context->InputData.GetInputs().IsEmpty()) { return false; } //Get rid of errors and warning when there is no input

	if (Context->MainPoints->IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingPoints", "Missing Input Points."));
		return false;
	}

	return true;
}

void FPCGExPointsProcessorElementBase::InitializeContext(
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
	InContext->MainPoints->DefaultOutputLabel = Settings->GetMainPointsOutputLabel();

	TArray<FPCGTaggedData> Sources = InContext->InputData.GetInputsByPin(Settings->GetMainPointsInputLabel());
	InContext->MainPoints->Initialize(
		InContext, Sources, Settings->GetPointOutputInitMode(),
		[&InContext](UPCGPointData* Data) { return InContext->ValidatePointDataInput(Data); },
		[&InContext](PCGExData::FPointIO& PointIO) { return InContext->PostInitPointDataInput(PointIO); });
}

#undef LOCTEXT_NAMESPACE
