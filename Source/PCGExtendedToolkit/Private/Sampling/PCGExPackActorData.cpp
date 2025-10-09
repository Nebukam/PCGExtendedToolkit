// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExPackActorData.h"

#include "PCGComponent.h"
#include "PCGExPointsProcessor.h"
#include "PCGParamData.h"
#include "Elements/PCGExecuteBlueprint.h"
#include "Engine/AssetManager.h"

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

void UPCGExCustomActorDataPacker::Initialize_Implementation(bool& OutSuccess)
{
}

void UPCGExCustomActorDataPacker::ProcessEntry_Implementation(AActor* InActor, const FPCGPoint& InPoint, const int32 InPointIndex, FPCGPoint& OutPoint)
{
}

#define PCGEX_SET_ATT_IMPL(_NAME, _TYPE)\
bool UPCGExCustomActorDataPacker::Init##_NAME(const FName& InAttributeName, const _TYPE& InValue){\
TSharedPtr<PCGExData::TBuffer<_TYPE>> Buffer = WriteBuffers->GetBuffer<_TYPE>(InAttributeName, InValue);\
return Buffer ? true : false;}

void UPCGExCustomActorDataPacker::AddComponent(
	AActor* InActor,
	TSubclassOf<UActorComponent> ComponentClass,
	EAttachmentRule InLocationRule,
	EAttachmentRule InRotationRule,
	EAttachmentRule InScaleRule,
	bool InWeldSimulatedBodies,
	UActorComponent*& OutComponent)
{
	if (!IsInGameThread())
	{
		UE_LOG(LogPCGEx, Error, TEXT("AddComponent can only be used on the game thread. Enable `bExecuteOnMainThread` on your packer!"));
		return;
	}

	if (!IsValid(InActor))
	{
		UE_LOG(LogPCGEx, Error, TEXT("AddComponent target actor is NULL"));
		return;
	}

	if (!ComponentClass || ComponentClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogPCGEx, Error, TEXT("AddComponent cannot instantiate an abstract class"));
		return;
	}

	const EObjectFlags InObjectFlags = (bIsPreviewMode ? RF_Transient : RF_NoFlags);
	OutComponent = Context->ManagedObjects->New<UActorComponent>(
		InActor, ComponentClass,
		UniqueNameGenerator->Get(TEXT("PCGComponent_") + ComponentClass->GetName()), InObjectFlags);

	if (!OutComponent)
	{
		UE_LOG(LogPCGEx, Error, TEXT("AddComponent could not instantiate component, something went wrong."));
		return;
	}

	{
		FComponentInfos NewInfos = FComponentInfos(
			OutComponent,
			InLocationRule,
			InRotationRule,
			InScaleRule,
			InWeldSimulatedBodies);

		Context->AttachManagedComponent(InActor, NewInfos.Component, NewInfos.AttachmentTransformRules);
	}
}

PCGEX_FOREACH_PACKER(PCGEX_SET_ATT_IMPL)
#undef PCGEX_SET_ATT_IMPL


bool UPCGExCustomActorDataPacker::InitSoftObjectPath(const FName& InAttributeName, const FSoftObjectPath& InValue)
{
	TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> Buffer = WriteBuffers->GetBuffer<FSoftObjectPath>(InAttributeName, InValue);
	return Buffer ? true : false;
}

bool UPCGExCustomActorDataPacker::InitSoftClassPath(const FName& InAttributeName, const FSoftClassPath& InValue)
{
	TSharedPtr<PCGExData::TBuffer<FSoftClassPath>> Buffer = WriteBuffers->GetBuffer<FSoftClassPath>(InAttributeName, InValue);
	return Buffer ? true : false;
}

void UPCGExCustomActorDataPacker::PreloadObjectPaths(const FName& InAttributeName)
{
	if (bIsProcessing)
	{
		PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("You may only call PreloadObjectPaths during initialization."));
		return;
	}

	TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(PrimaryDataFacade->Source->GetIn()->Metadata);
	const PCGEx::FAttributeIdentity* Identity = Infos->Find(InAttributeName);

	if (!Identity)
	{
		PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("Preload attribute \"{0}\" does not exist."), FText::FromName(InAttributeName)));
		return;
	}

	if (Identity->UnderlyingType == EPCGMetadataTypes::String)
	{
		if (TSharedPtr<PCGExData::TArrayBuffer<FString>> Buffer = ReadBuffers->GetBuffer<FString>(InAttributeName))
		{
			const TArray<FString>& Values = *Buffer->GetInValues().Get();
			for (const FString& V : Values) { RequiredAssetsPaths.Add(FSoftObjectPath(V)); }
		}
	}

	if (Identity->UnderlyingType == EPCGMetadataTypes::SoftObjectPath)
	{
		if (TSharedPtr<PCGExData::TArrayBuffer<FSoftObjectPath>> Buffer = ReadBuffers->GetBuffer<FSoftObjectPath>(InAttributeName))
		{
			const TArray<FSoftObjectPath>& Values = *Buffer->GetInValues().Get();
			for (const FSoftObjectPath& V : Values) { RequiredAssetsPaths.Add(V); }
		}
	}
}

#define PCGEX_SET_ATT_IMPL(_NAME, _TYPE)\
bool UPCGExCustomActorDataPacker::Write##_NAME(const FName& InAttributeName, const int32 InPointIndex, const _TYPE& InValue){\
return WriteBuffers->SetValue<_TYPE>(InAttributeName, InPointIndex, InValue);}
PCGEX_FOREACH_PACKER(PCGEX_SET_ATT_IMPL)
#undef PCGEX_SET_ATT_IMPL

bool UPCGExCustomActorDataPacker::WriteSoftObjectPath(const FName& InAttributeName, const int32 InPointIndex, const FSoftObjectPath& InValue)
{
	return WriteBuffers->SetValue<FSoftObjectPath>(InAttributeName, InPointIndex, InValue);
}

bool UPCGExCustomActorDataPacker::WriteSoftClassPath(const FName& InAttributeName, const int32 InPointIndex, const FSoftClassPath& InValue)
{
	return WriteBuffers->SetValue<FSoftClassPath>(InAttributeName, InPointIndex, InValue);
}

#define PCGEX_SET_ATT_IMPL(_NAME, _TYPE)\
bool UPCGExCustomActorDataPacker::Read##_NAME(const FName& InAttributeName, const int32 InPointIndex, _TYPE& OutValue){\
return ReadBuffers->GetValue<_TYPE>(InAttributeName, InPointIndex, OutValue);}
PCGEX_FOREACH_PACKER(PCGEX_SET_ATT_IMPL)
#undef PCGEX_SET_ATT_IMPL

bool UPCGExCustomActorDataPacker::ReadSoftObjectPath(const FName& InAttributeName, const int32 InPointIndex, FSoftObjectPath& OutValue)
{
	return ReadBuffers->GetValue<FSoftObjectPath>(InAttributeName, InPointIndex, OutValue);
}

bool UPCGExCustomActorDataPacker::ReadSoftClassPath(const FName& InAttributeName, const int32 InPointIndex, FSoftClassPath& OutValue)
{
	return ReadBuffers->GetValue<FSoftClassPath>(InAttributeName, InPointIndex, OutValue);
}

void UPCGExCustomActorDataPacker::ResolveObjectPath(const FName& InAttributeName, const int32 InPointIndex, TSubclassOf<UObject> OutObjectClass, UObject*& OutObject, bool& OutIsValid)
{
	OutIsValid = false;
	FSoftObjectPath InSoftObjectPath;
	if (ReadSoftObjectPath(InAttributeName, InPointIndex, InSoftObjectPath))
	{
		UObject* ResolvedObject = InSoftObjectPath.ResolveObject();
		if (ResolvedObject && ResolvedObject->IsA(OutObjectClass))
		{
			OutObject = ResolvedObject;
			OutIsValid = true;
		}
	}
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
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExPackActorData::SourceOverridesPacker)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPackActorDataSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_PARAMS(TEXT("AttributeSet"), "Same as point, but contains only added data.", Advanced)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(PackActorData)
PCGEX_ELEMENT_BATCH_POINT_IMPL(PackActorData)

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

	InContext->EDITOR_TrackClass(Settings->Packer->GetClass());

	PCGEX_OPERATION_BIND(Packer, UPCGExCustomActorDataPacker, PCGExPackActorData::SourceOverridesPacker)
	PCGEX_VALIDATE_NAME_CONSUMABLE(Settings->ActorReferenceAttribute)

	//Context->OutputParams.Init(nullptr, Context->MainPoints->Num());

	return true;
}

bool FPCGExPackActorDataElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPackActorDataElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PackActorData)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
				NewBatch->PrimaryInstancedFactory = Context->Packer;
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();

	/*
	for (int i = 0; i < Context->MainPoints->Pairs.Num(); i++)
	{
		UPCGParamData* ParamData = Context->OutputParams[i];
		if (!ParamData) { continue; }
		Context->StageOutput(TEXT("AttributeSet"), ParamData, Context->MainPoints->Pairs[i]->Tags->Flatten(), false, false);
	}
	*/

	return Context->TryComplete();
}

namespace PCGExPackActorData
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPackActorData::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }


		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		PointMask.Init(1, PointDataFacade->GetNum());

		Packer = GetPrimaryInstancedFactory<UPCGExCustomActorDataPacker>();
		Packer->UniqueNameGenerator = Context->UniqueNameGenerator;
		Packer->WriteBuffers = MakeShared<PCGExData::TBufferHelper<PCGExData::EBufferHelperMode::Write>>(PointDataFacade);
		Packer->ReadBuffers = MakeShared<PCGExData::TBufferHelper<PCGExData::EBufferHelperMode::Read>>(PointDataFacade);

		Packer->bIsPreviewMode = ExecutionContext->GetComponent()->IsInPreviewMode();

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

		{
			if (!IsInGameThread())
			{
				FGCScopeGuard Scope;
				Packer->Initialize(bSuccess);
			}
			else
			{
				Packer->Initialize(bSuccess);
			}
		}

		if (!bSuccess)
		{
			if (!Settings->bQuietUninitializedPackerWarning)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some data could not be initialized. Make sure to override the packer 'Initialize' so it returns true. If that's intended, you can mute this warning in the node settings."));
			}

			return false;
		}

		if (Packer->RequiredAssetsPaths.IsEmpty())
		{
			StartProcessing();
		}
		else
		{
			LoadToken = AsyncManager->TryCreateToken(FName("Asset Loading"));
			if (!LoadToken.IsValid()) { return false; }

			AsyncTask(
				ENamedThreads::GameThread, [PCGEX_ASYNC_THIS_CAPTURE]()
				{
					PCGEX_ASYNC_THIS

					if (!This->LoadToken.IsValid()) { return; }

					This->LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
						This->Packer->RequiredAssetsPaths.Array(), [AsyncThis]()
						{
							PCGEX_ASYNC_NESTED_THIS

							if (!NestedThis->LoadToken.IsValid()) { return; }

							NestedThis->StartProcessing();
							PCGEX_ASYNC_RELEASE_TOKEN(NestedThis->LoadToken)
						});

					if (!This->LoadHandle || !This->LoadHandle->IsActive())
					{
						This->StartProcessing();
						PCGEX_ASYNC_RELEASE_TOKEN(This->LoadToken)
					}
				});
		}

		return true;
	}

	void FProcessor::StartProcessing()
	{
		Packer->bIsProcessing = true;
		if (Settings->Packer->bExecuteOnMainThread)
		{
			GetPoints(PointDataFacade->GetOutFullScope(), PointsForProcessing);

			MainThreadLoop = MakeShared<PCGExMT::FScopeLoopOnMainThread>(PointDataFacade->GetNum());
			MainThreadLoop->OnIterationCallback =
				[&](const int32 Index, const PCGExMT::FScope& Scope)
				{
					AActor* ActorRef = Packer->InputActors[Index];
					if (!ActorRef)
					{
						PointMask[Index] = 0;
						return;
					}

					FPCGPoint& Point = PointsForProcessing[Index];
					Packer->ProcessEntry(ActorRef, Point, Index, Point);
				};

			PCGEX_ASYNC_HANDLE_CHKD_VOID(AsyncManager, MainThreadLoop)
		}
		else
		{
			StartParallelLoopForPoints();
		}
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::PackActorData::ProcessPoints);

		TArray<FPCGPoint> LocalPointsForProcessing;
		GetPoints(PointDataFacade->GetOutScope(Scope), LocalPointsForProcessing);

		int i = -1;
		PCGEX_SCOPE_LOOP(Index)
		{
			i++;
			AActor* ActorRef = Packer->InputActors[Index];
			if (!ActorRef)
			{
				PointMask[Index] = 0;
				continue;
			}

			FPCGPoint& Point = LocalPointsForProcessing[i];
			Packer->ProcessEntry(ActorRef, Point, Index, Point);
		}

		PointDataFacade->Source->SetPoints(Scope.Start, LocalPointsForProcessing, EPCGPointNativeProperties::All);
	}

	void FProcessor::CompleteWork()
	{
		if (Settings->Packer->bExecuteOnMainThread)
		{
			PointDataFacade->Source->SetPoints(0, PointsForProcessing, EPCGPointNativeProperties::All);
			PointsForProcessing.Empty();
		}

		Attributes.Reserve(PointDataFacade->Buffers.Num());
		for (const TSharedPtr<PCGExData::IBuffer>& Buffer : PointDataFacade->Buffers)
		{
			if (!Buffer->IsWritable()) { continue; }
			Attributes.Add(Buffer->OutAttribute);
		}

		PointDataFacade->WriteFastest(AsyncManager);
	}

	void FProcessor::Write()
	{
		if (Settings->bOmitUnresolvedEntries) { (void)PointDataFacade->Source->Gather(PointMask); }

		/*
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
						using T_REAL = decltype(DummyValue);
						FPCGMetadataAttribute<T_REAL>* A = static_cast<FPCGMetadataAttribute<T_REAL>*>(OutAttribute);
						FPCGMetadataAttribute<T_REAL>* B = ParamMetadata->FindOrCreateAttribute(A->Name, A->GetValueFromItemKey(PCGDefaultValueKey), A->AllowsInterpolation());
						B->SetValue(Key, A->GetValueFromItemKey(ItemKey));
					});
			}
		}
		*/
	}
	
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
