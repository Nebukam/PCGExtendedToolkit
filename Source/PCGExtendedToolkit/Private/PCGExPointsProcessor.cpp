// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPointsProcessor.h"

#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

UPCGExPointsProcessorSettings::UPCGExPointsProcessorSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (ChunkSize <= 0) { ChunkSize = UPCGExPointsProcessorSettings::GetPreferredChunkSize(); }
}

TArray<FPCGPinProperties> UPCGExPointsProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertySource = PinProperties.Emplace_GetRef(GetMainPointsInputLabel(), EPCGDataType::Point);

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

PCGExIO::EInitMode UPCGExPointsProcessorSettings::GetPointOutputInitMode() const { return PCGExIO::EInitMode::NewOutput; }

int32 UPCGExPointsProcessorSettings::GetPreferredChunkSize() const { return 256; }

PCGEx::FPinAttributeInfos* UPCGExPointsProcessorSettings::GetInputAttributeInfos(FName PinLabel)
{
	PCGEx::FPinAttributeInfos* Infos = AttributesMap.Find(PinLabel);
	if (!Infos)
	{
		AttributesMap.Add(PinLabel, PCGEx::FPinAttributeInfos());
		Infos = AttributesMap.Find(PinLabel);
		Infos->PinLabel = PinLabel;
	}

	return Infos;
}

bool FPCGExPointsProcessorContext::AdvancePointsIO()
{
	CurrentPointsIndex++;
	if (MainPoints->Pairs.IsValidIndex(CurrentPointsIndex))
	{
		CurrentIO = MainPoints->Pairs[CurrentPointsIndex];
		return true;
	}
	CurrentIO = nullptr;
	return false;
}

void FPCGExPointsProcessorContext::SetState(PCGExMT::EState OperationId)
{
	PCGExMT::EState PreviousOperation = CurrentState;
	CurrentState = OperationId;
}

void FPCGExPointsProcessorContext::Reset() { CurrentState = PCGExMT::EState::Setup; }

bool FPCGExPointsProcessorContext::ValidatePointDataInput(UPCGPointData* PointData) { return true; }

void FPCGExPointsProcessorContext::PostInitPointDataInput(UPCGExPointIO* PointData)
{
}

#pragma endregion

bool FPCGExPointsProcessorContext::AsyncProcessingMainPoints(
	TFunction<void(UPCGExPointIO*)>&& Initialize,
	TFunction<void(int32, UPCGExPointIO*)>&& LoopBody)
{
	const int32 NumPairs = MainPoints->Pairs.Num();

	if (!bProcessingMainPoints)
	{
		bProcessingMainPoints = true;
		MainPointsPairProcessingStatuses.Empty(NumPairs);
		for (int i = 0; i < NumPairs; i++) { MainPointsPairProcessingStatuses.Add(false); }
	}

	int32 NumPairsDone = 0;

	for (int i = 0; i < NumPairs; i++)
	{
		bool bState = MainPointsPairProcessingStatuses[i];
		if (!bState)
		{
			//bState = Pairs[i]->InputParallelProcessing(Context, Initialize, LoopBody, ChunkSize, bForceSync);
			bState = PCGExMT::ParallelForLoop(
				this, MainPoints->Pairs[i]->NumInPoints,
				[&]() { Initialize(MainPoints->Pairs[i]); },
				[&](int32 Index) { LoopBody(Index, MainPoints->Pairs[i]); }, ChunkSize, !bDoAsyncProcessing);
			if (bState) { MainPointsPairProcessingStatuses[i] = bState; }
		}
		if (bState) { NumPairsDone++; }
	}

	if (NumPairs == NumPairsDone)
	{
		bProcessingMainPoints = false;
		return true;
	}
	else
	{
		return false;
	}
}

bool FPCGExPointsProcessorContext::AsyncProcessingCurrentPoints(
	TFunction<void(UPCGExPointIO*)>&& Initialize,
	TFunction<void(const int32, const UPCGExPointIO*)>&& LoopBody)
{
	if (!bDoAsyncProcessing)
	{
		Initialize(CurrentIO);
		for (int i = 0; i < CurrentIO->NumInPoints; i++) { LoopBody(i, CurrentIO); }
		return true;
	};
	return FPCGAsync::AsyncProcessingOneToOneEx(
		&AsyncState, CurrentIO->NumInPoints,
		[&]() { Initialize(CurrentIO); },
		[&](int32 ReadIndex, int32 WriteIndex)
		{
			LoopBody(ReadIndex, CurrentIO);
			return true;
		}, true, ChunkSize);
}

bool FPCGExPointsProcessorContext::AsyncProcessingCurrentPoints(TFunction<void(const int32, const UPCGExPointIO*)>&& LoopBody)
{
	if (!bDoAsyncProcessing)
	{
		for (int i = 0; i < CurrentIO->NumInPoints; i++) { LoopBody(i, CurrentIO); }
		return true;
	};
	return FPCGAsync::AsyncProcessingOneToOneEx(
		&AsyncState, CurrentIO->NumInPoints,
		[&]()
		{
		},
		[&](int32 ReadIndex, int32 WriteIndex)
		{
			LoopBody(ReadIndex, CurrentIO);
			return true;
		}, true, ChunkSize);
}

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

	InContext->bDoAsyncProcessing = Settings->bDoAsyncProcessing;
	InContext->ChunkSize = FMath::Max(Settings->ChunkSize, 1);

	InContext->MainPoints = NewObject<UPCGExPointIOGroup>();
	InContext->MainPoints->DefaultOutputLabel = Settings->GetMainPointsOutputLabel();

	TArray<FPCGTaggedData> Sources = InContext->InputData.GetInputsByPin(Settings->GetMainPointsInputLabel());
	InContext->MainPoints->Initialize(
		InContext,
		Sources,
		Settings->GetPointOutputInitMode(),
		[&InContext](UPCGPointData* Data) { return InContext->ValidatePointDataInput(Data); },
		[&InContext](UPCGExPointIO* PointIO) { return InContext->PostInitPointDataInput(PointIO); });

	//	UPCGExPointsProcessorSettings* MutableSettings = const_cast<UPCGExPointsProcessorSettings*>(Settings);
	//	PCGEx::FPinAttributeInfos* SourceInfos = MutableSettings->GetInputAttributeInfos(Settings->GetMainPointsInputLabel());
	//	SourceInfos->Reset();
	//	for (const UPCGExPointIO* PointIO : InContext->Points->Pairs) { SourceInfos->Discover(PointIO->In); }
}

#undef LOCTEXT_NAMESPACE
