// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExPackActorData.h"

#include "PCGExPointsProcessor.h"
#include "Misc/PCGExSortPoints.h"


#define LOCTEXT_NAMESPACE "PCGExPackActorDataElement"
#define PCGEX_NAMESPACE PackActorData

// Convenient macro to avoid duplicating a lot of code with all our supported types.
#define PCGEX_FOREACH_PACKER(MACRO) \
MACRO(Int32, int32) \
MACRO(Int64, int64) \
MACRO(Float, float) \
MACRO(Double, double) \
MACRO(Vector2, FVector2D) \
MACRO(Vector, FVector) \
MACRO(Vector4, FVector4) \
MACRO(Quat, FQuat) \
MACRO(Transform, FTransform) \
MACRO(String, FString) \
MACRO(Bool, bool) \
MACRO(Rotator, FRotator) \
MACRO(Name, FName)
//MACRO(SoftObjectPath, FSoftObjectPath) \
//MACRO(SoftClassPath, FSoftClassPath)

void UPCGExCustomActorDataPacker::InitializeWithContext_Implementation(const FPCGContext& InContext, bool& OutSuccess)
{
}

void UPCGExCustomActorDataPacker::ProcessEntry_Implementation(AActor* InActor, const FPCGPoint& InPoint, const int32 InPointIndex, FPCGPoint& OutPoint)
{
}

#define PCGEX_SET_ATT_IMPL(_NAME, _TYPE)\
bool UPCGExCustomActorDataPacker::Init##_NAME(const FName& InAttributeName, const _TYPE& InValue){\
TSharedPtr<PCGExData::TBuffer<_TYPE>> Buffer = Buffers->GetBuffer<_TYPE>(InAttributeName, InValue);\
return Buffer ? true : false;}
PCGEX_FOREACH_PACKER(PCGEX_SET_ATT_IMPL)
#undef PCGEX_SET_ATT_IMPL


bool UPCGExCustomActorDataPacker::InitSoftObjectPath(const FName& InAttributeName, const FSoftObjectPath& InValue)
{
#if PCGEX_ENGINE_VERSION <= 503
	TSharedPtr<PCGExData::TBuffer<FString>> Buffer = Buffers->GetBuffer<FString>(InAttributeName, InValue.ToString());
#else
	TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> Buffer = Buffers->GetBuffer<FSoftObjectPath>(InAttributeName, InValue);
#endif
	return Buffer ? true : false;
}

bool UPCGExCustomActorDataPacker::InitSoftClassPath(const FName& InAttributeName, const FSoftClassPath& InValue)
{
#if PCGEX_ENGINE_VERSION <= 503
	TSharedPtr<PCGExData::TBuffer<FString>> Buffer = Buffers->GetBuffer<FString>(InAttributeName, InValue.ToString());
#else
	TSharedPtr<PCGExData::TBuffer<FSoftClassPath>> Buffer = Buffers->GetBuffer<FSoftClassPath>(InAttributeName, InValue);
#endif
	return Buffer ? true : false;
}

#define PCGEX_SET_ATT_IMPL(_NAME, _TYPE)\
bool UPCGExCustomActorDataPacker::Pack##_NAME(const FName& InAttributeName, const int32 InPointIndex, const _TYPE& InValue){\
return Buffers->SetValue<_TYPE>(InAttributeName, InPointIndex, InValue);}
PCGEX_FOREACH_PACKER(PCGEX_SET_ATT_IMPL)
#undef PCGEX_SET_ATT_IMPL

bool UPCGExCustomActorDataPacker::PackSoftObjectPath(const FName& InAttributeName, const int32 InPointIndex, const FSoftObjectPath& InValue)
{
#if PCGEX_ENGINE_VERSION <= 503
	return Buffers->SetValue<FString>(InAttributeName, InPointIndex, InValue.ToString());
#else
	return Buffers->SetValue<FSoftObjectPath>(InAttributeName, InPointIndex, InValue);
#endif
}

bool UPCGExCustomActorDataPacker::PackSoftClassPath(const FName& InAttributeName, const int32 InPointIndex, const FSoftClassPath& InValue)
{
#if PCGEX_ENGINE_VERSION <= 503
	return Buffers->SetValue<FString>(InAttributeName, InPointIndex, InValue.ToString());
#else
	return Buffers->SetValue<FSoftClassPath>(InAttributeName, InPointIndex, InValue);
#endif
}

#undef PCGEX_FOREACH_PACKER


UPCGExPackActorDataSettings::UPCGExPackActorDataSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPCGExPackActorDataSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_DEPENDENCIES
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExPackActorDatas::SourceOverridesPacker)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPackActorDataSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_PARAMS(TEXT("AttributeSet"), "Same as point, but contains only added data.", Advanced, {})
	return PinProperties;
}

PCGExData::EIOInit UPCGExPackActorDataSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

int32 UPCGExPackActorDataSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_L; }

PCGEX_INITIALIZE_ELEMENT(PackActorData)

FName UPCGExPackActorDataSettings::GetMainInputPin() const
{
	return PCGEx::SourceTargetsLabel;
}

bool FPCGExPackActorDataElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PackActorData)

	if (!Settings->Packer)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No builder selected."));
		return false;
	}

	PCGEX_OPERATION_BIND(Packer, UPCGExCustomActorDataPacker, PCGExPackActorDatas::SourceOverridesPacker)
	PCGEX_VALIDATE_NAME(Settings->ActorReferenceAttribute)

	Context->OutputParams.Init(nullptr, Context->MainPoints->Num());

	return true;
}

bool FPCGExPackActorDataElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPackActorDataElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PackActorData)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPackActorDatas::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExPackActorDatas::FProcessor>>& NewBatch)
			{
				NewBatch->PrimaryOperation = Context->Packer;
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	for (int i = 0; i < Context->MainPoints->Pairs.Num(); i++)
	{
		UPCGParamData* ParamData = Context->OutputParams[i];
		if (!ParamData) { continue; }
		Context->StageOutput(TEXT("AttributeSet"), ParamData, Context->MainPoints->Pairs[i]->Tags->ToSet(), false, false);
	}

	return Context->TryComplete();
}

namespace PCGExPackActorDatas
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPackActorDatas::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		Packer = static_cast<UPCGExCustomActorDataPacker*>(PrimaryOperation);
		Packer->Buffers = MakeShared<PCGExData::FBufferHelper>(PointDataFacade);

		PointDataFacade->Source->bAllowEmptyOutput = !Settings->bOmitEmptyOutputs;

		ActorReferences = MakeShared<PCGEx::TAttributeBroadcaster<FSoftObjectPath>>();
		if (!ActorReferences->Prepare(Settings->ActorReferenceAttribute, PointDataFacade->Source))
		{
			PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some inputs don't have the specified Actor Reference attribute."));
			return false;
		}

		ActorReferences->Grab();
		Packer->InputActors.Init(nullptr, PointDataFacade->GetNum());

		for (int i = 0; i < ActorReferences->Values.Num(); i++) { Packer->InputActors[i] = Cast<AActor>(ActorReferences->Values[i].ResolveObject()); }

		bool bSuccess = false;
		Packer->InitializeWithContext(*Context, bSuccess);

		if (!bSuccess) { return false; }

		StartParallelLoopForPoints();
		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		AActor* ActorRef = Packer->InputActors[Index];
		if (!ActorRef) { return; }

		Packer->ProcessEntry(ActorRef, Point, Index, Point);
	}

	void FProcessor::CompleteWork()
	{
		Attributes.Reserve(PointDataFacade->Buffers.Num());
		for (const TSharedPtr<PCGExData::FBufferBase>& Buffer : PointDataFacade->Buffers)
		{
			if (!Buffer->IsWritable()) { continue; }
			Attributes.Add(Buffer->OutAttribute);
		}

		PointDataFacade->Write(AsyncManager);
	}

	void FProcessor::Write()
	{
		TArray<int32> ValidIndices;
		PCGEx::ArrayOfIndices(ValidIndices, PointDataFacade->Source->GetNum());

		if (Settings->bOmitUnresolvedEntries)
		{
			TArray<FPCGPoint>& MutablePoints = PointDataFacade->GetOut()->GetMutablePoints();
			const int32 NumPoints = MutablePoints.Num();
			int32 WriteIndex = 0;
			for (int32 i = 0; i < NumPoints; i++)
			{
				if (Packer->InputActors[i])
				{
					ValidIndices[WriteIndex] = i;
					MutablePoints[WriteIndex++] = MutablePoints[i];
				}
			}

			if (MutablePoints.IsEmpty() && Settings->bOmitEmptyOutputs) { return; }

			ValidIndices.SetNum(WriteIndex);
			MutablePoints.SetNum(WriteIndex);
		}

		UPCGParamData* ParamData = Context->ManagedObjects->New<UPCGParamData>();
		Context->OutputParams[PointDataFacade->Source->IOIndex] = ParamData;
		TArray<FPCGPoint>& MutablePoints = PointDataFacade->GetOut()->GetMutablePoints();

		UPCGMetadata* ParamMetadata = ParamData->Metadata;

		for (int i = 0; i < MutablePoints.Num(); i++)
		{
			const int64 Key = ParamMetadata->AddEntry();
			PCGMetadataEntryKey ItemKey = MutablePoints[i].MetadataEntry;
			const int32 ValidIndex = ValidIndices[i];

			if (!Packer->InputActors[ValidIndex]) { continue; }

			for (FPCGMetadataAttributeBase* OutAttribute : Attributes)
			{
					PCGEx::ExecuteWithRightType(
                                                    						OutAttribute->GetTypeId(), [&](auto DummyValue)
                                                    						{
						using RawT = decltype(DummyValue);
						FPCGMetadataAttribute<RawT>* A = static_cast<FPCGMetadataAttribute<RawT>*>(OutAttribute);
						FPCGMetadataAttribute<RawT>* B = ParamMetadata->FindOrCreateAttribute(A->Name, A->GetValueFromItemKey(PCGDefaultValueKey), A->AllowsInterpolation());
						B->SetValue(Key, A->GetValueFromItemKey(ItemKey));
					});
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
