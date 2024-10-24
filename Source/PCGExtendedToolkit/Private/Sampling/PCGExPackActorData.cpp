// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExPackActorData.h"

#include "PCGExPointsProcessor.h"
#include "Data/Blending/PCGExMetadataBlender.h"
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
bool UPCGExCustomActorDataPacker::Pack##_NAME(const FName& InAttributeName, const int32 InPointIndex, const _TYPE& InValue){\
TSharedPtr<PCGExData::TBuffer<_TYPE>> Buffer = PointDataFacade->GetWritable<_TYPE>(InAttributeName, false);\
if (!Buffer) { return false; }\
Buffer->GetMutable(InPointIndex) = InValue;\
return true;}
PCGEX_FOREACH_PACKER(PCGEX_SET_ATT_IMPL)

bool UPCGExCustomActorDataPacker::PackSoftObjectPath(const FName& InAttributeName, const int32 InPointIndex, const FSoftObjectPath& InValue)
{
#if PCGEX_ENGINE_VERSION <= 503
	return PackString(InAttributeName, InPointIndex, InValue.ToString());
#else
	TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> Buffer = PointDataFacade->GetWritable<FSoftObjectPath>(InAttributeName, false);
	if (!Buffer) { return false; }
	Buffer->GetMutable(InPointIndex) = InValue;
	return true;
#endif
}

bool UPCGExCustomActorDataPacker::PackSoftClassPath(const FName& InAttributeName, const int32 InPointIndex, const FSoftClassPath& InValue)
{
#if PCGEX_ENGINE_VERSION <= 503
	return PackString(InAttributeName, InPointIndex, InValue.ToString());
#else
	TSharedPtr<PCGExData::TBuffer<FSoftClassPath>> Buffer = PointDataFacade->GetWritable<FSoftClassPath>(InAttributeName, false);
	if (!Buffer) { return false; }
	Buffer->GetMutable(InPointIndex) = InValue;
	return true;
#endif
}

#undef PCGEX_SET_ATT_IMPL
#undef PCGEX_FOREACH_PACKER


UPCGExPackActorDataSettings::UPCGExPackActorDataSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExData::EInit UPCGExPackActorDataSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

int32 UPCGExPackActorDataSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_L; }

PCGEX_INITIALIZE_ELEMENT(PackActorData)

FName UPCGExPackActorDataSettings::GetMainInputLabel() const
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

	PCGEX_OPERATION_BIND(Packer, UPCGExCustomActorDataPacker)
	PCGEX_VALIDATE_NAME(Settings->ActorReferenceAttribute)

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
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

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
		Packer->PointDataFacade = PointDataFacade;

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
		PointDataFacade->Write(AsyncManager);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
