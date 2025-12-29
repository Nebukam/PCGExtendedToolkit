// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExWaitForPCGData.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Helpers/PCGExStreamingHelpers.h"
#include "PCGGraph.h"
#include "PCGSubsystem.h"
#include "PCGExSubSystem.h"
#include "Tasks/Task.h"
#include "Async/Async.h"
#include "Data/PCGExAttributeBroadcaster.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Utils/PCGExIntTracker.h"

#define LOCTEXT_NAMESPACE "PCGExWaitForPCGDataElement"
#define PCGEX_NAMESPACE WaitForPCGData

UPCGExWaitForPCGDataSettings::UPCGExWaitForPCGDataSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
void UPCGExWaitForPCGDataSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UPCGExWaitForPCGDataSettings, TemplateGraph))
	{
		EDITOR_RefreshPins();
	}
}
#endif

bool UPCGExWaitForPCGDataSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	return true;
}

TArray<FPCGPinProperties> UPCGExWaitForPCGDataSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	FPCGPinProperties& DependencyPin = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultExecutionDependencyLabel, EPCGDataType::Any, /*bInAllowMultipleConnections=*/true, /*bAllowMultipleData=*/true);
	DependencyPin.Usage = EPCGPinUsage::DependencyOnly;

	if (bOutputRoaming) { PCGEX_PIN_ANY(RoamingPin, "Roaming data that isn't part of the template output but still exists.", Normal) }

	PinProperties.Append(CachedPins);

	return PinProperties;
}

#if WITH_EDITOR
void UPCGExWaitForPCGDataSettings::EDITOR_RefreshPins()
{
	Modify(true);

	GetTargetGraphPins(CachedPins); // Force-refresh cached pins

	FPropertyChangedEvent EmptyEvent(nullptr);
	PostEditChangeProperty(EmptyEvent);
	MarkPackageDirty();
}
#endif

void FPCGExWaitForPCGDataContext::RegisterAssetDependencies()
{
	PCGEX_SETTINGS_LOCAL(WaitForPCGData)

	GraphInstancePaths.Reserve(MainPoints->Pairs.Num());

	if (Settings->TemplateInput == EPCGExDataInputValueType::Attribute)
	{
		for (const TSharedPtr<PCGExData::FPointIO>& IO : MainPoints->Pairs)
		{
			FSoftObjectPath Path = FSoftObjectPath();
			if (PCGExData::Helpers::TryReadDataValue(this, IO->GetIn(), Settings->TemplateGraphAttributeName, Path)) { AddAssetDependency(Path); }
			GraphInstancePaths.Add(Path);
		}
	}
	else
	{
		FSoftObjectPath Path = Settings->TemplateGraph.ToSoftObjectPath();
		AddAssetDependency(Path);
		for (const TSharedPtr<PCGExData::FPointIO>& IO : MainPoints->Pairs) { GraphInstancePaths.Add(Path); }
	}

	FPCGExPointsProcessorContext::RegisterAssetDependencies();
}

PCGEX_INITIALIZE_ELEMENT(WaitForPCGData)
PCGEX_ELEMENT_BATCH_POINT_IMPL(WaitForPCGData)

void UPCGExWaitForPCGDataSettings::GetTargetGraphPins(TArray<FPCGPinProperties>& OutPins) const
{
	OutPins.Empty();
	PCGExHelpers::LoadBlocking_AnyThreadTpl(TemplateGraph);
	if (const UPCGGraph* PinPropertiesSource = TemplateGraph.Get())
	{
		TArray<FPCGPinProperties> FoundPins = PinPropertiesSource->GetOutputNode()->OutputPinProperties();
		OutPins.Reserve(FoundPins.Num());
		for (FPCGPinProperties Pin : FoundPins)
		{
			Pin.bInvisiblePin = false;
			OutPins.Add(Pin);
		}
	}
}

FName UPCGExWaitForPCGDataSettings::GetMainInputPin() const
{
	return PCGExCommon::Labels::SourceTargetsLabel;
}

bool FPCGExWaitForPCGDataElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WaitForPCGData)

	PCGEX_VALIDATE_NAME_CONSUMABLE(Settings->ActorReferenceAttribute)
	if (Settings->TemplateInput == EPCGExDataInputValueType::Attribute) { PCGEX_VALIDATE_NAME(Settings->TemplateGraphAttributeName) }

	for (FPCGPinProperties Pin : Settings->CachedPins)
	{
		Context->AllLabels.Add(Pin.Label);

		if (Pin.IsRequiredPin())
		{
			Context->RequiredPinProperties.Add(Pin);
			Context->RequiredLabels.Add(Pin.Label);
		}
	}

	return true;
}

bool FPCGExWaitForPCGDataElement::PostBoot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::PostBoot(InContext)) { return false; }

	FPCGExWaitForPCGDataContext* Context = static_cast<FPCGExWaitForPCGDataContext*>(InContext);
	for (const FSoftObjectPath& Path : Context->GraphInstancePaths)
	{
		TSoftObjectPtr<UPCGGraph> Graph = TSoftObjectPtr<UPCGGraph>(Path);
		if (!Graph.Get())
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Some graph could not be loaded."));
			return false;
		}
		Context->GraphInstances.Add(Graph.Get());
	}

	return true;
}

bool FPCGExWaitForPCGDataElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWaitForPCGDataElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(WaitForPCGData)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints([&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; }, [&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
		{
		}))
		{
			return Context->CancelExecution(TEXT("Could not find any points."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	return Context->TryComplete();
}

namespace PCGExWaitForPCGData
{
	class FStageComponentDataTask final : public PCGExMT::FPCGExIndexedTask
	{
	public:
		explicit FStageComponentDataTask(const int32 InTaskIndex, const TWeakPtr<FProcessor>& InProcessor)
			: FPCGExIndexedTask(InTaskIndex), Processor(InProcessor)
		{
		}

		const TWeakPtr<FProcessor> Processor;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager) override
		{
			if (const TSharedPtr<FProcessor> P = Processor.Pin()) { P->StageComponentData(TaskIndex); }
		}
	};

	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWaitForPCGData::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }

		TemplateGraph = Context->GraphInstances[PointDataFacade->Source->IOIndex];

		TargetAttributesToDataTags = Settings->TargetAttributesToDataTags;
		if (Settings->bDedupeData) { TargetAttributesToDataTags.bAddIndexTag = false; }
		if (!TargetAttributesToDataTags.Init(Context, PointDataFacade)) { return false; }

		InspectionTracker = MakeShared<FPCGExIntTracker>([PCGEX_ASYNC_THIS_CAPTURE]()
		{
			PCGEX_ASYNC_THIS
			This->OnInspectionComplete();
		});

		WatcherTracker = MakeShared<FPCGExIntTracker>([PCGEX_ASYNC_THIS_CAPTURE]()
		                                              {
			                                              PCGEX_ASYNC_THIS
			                                              This->WatchToken = This->TaskManager->TryCreateToken(FName("Watch"));
		                                              }, [PCGEX_ASYNC_THIS_CAPTURE]()
		                                              {
			                                              PCGEX_ASYNC_THIS
			                                              PCGEX_ASYNC_RELEASE_TOKEN(This->WatchToken);
		                                              });

		PCGEX_MAKE_SHARED(ActorReferences, PCGExData::TAttributeBroadcaster<FSoftObjectPath>)

		if (!ActorReferences->Prepare(Settings->ActorReferenceAttribute, PointDataFacade->Source))
		{
			PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some inputs don't have the specified Actor Reference attribute."));
			return false;
		}

		ActorReferences->Grab();
		for (int i = 0; i < ActorReferences->Values.Num(); i++)
		{
			const FSoftObjectPath& ActorRef = ActorReferences->Values[i];
			if (!ActorRef.IsValid()) { continue; }

			bool bAlreadySet = false;
			UniqueActorReferences.Add(ActorRef, &bAlreadySet);

			if (bAlreadySet)
			{
				PerActorPoints[ActorRef]->Add(i);
			}
			else
			{
				PCGEX_MAKE_SHARED(PointList, TArray<int32>)
				PointList->Add(i);
				PerActorPoints.Add(ActorRef, PointList);
			}
		}

		QueuedActors.Reserve(UniqueActorReferences.Num());

		if (Settings->bWaitForMissingActors)
		{
			StartTime = Context->GetWorld()->GetTimeSeconds();
			SearchActorsToken = TaskManager->TryCreateToken(FName("SearchActors"));
			if (!SearchActorsToken.IsValid()) { return false; }
			GatherActors();
		}
		else
		{
			bool bHasUnresolvedReferences = false;

			for (const FSoftObjectPath& ActorRef : UniqueActorReferences)
			{
				if (AActor* Actor = Cast<AActor>(ActorRef.ResolveObject())) { QueuedActors.Add(Actor); }
				else { bHasUnresolvedReferences = true; }
			}

			if (QueuedActors.IsEmpty() && !Settings->bQuietActorNotFoundWarning)
			{
				PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Could not resolve any actor references."));
				return false;
			}

			if (bHasUnresolvedReferences && !Settings->bQuietActorNotFoundWarning)
			{
				PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some actor references could not be resolved."));
			}

			StartComponentSearch();
		}


		return true;
	}

	void FProcessor::GatherActors()
	{
		if (!SearchActorsToken.IsValid()) { return; }
		if (!TaskManager->IsAvailable())
		{
			PCGEX_ASYNC_RELEASE_TOKEN(SearchActorsToken)
			return;
		}

		bool bHasUnresolvedReferences = false;

		QueuedActors.Reset(UniqueActorReferences.Num());
		for (const FSoftObjectPath& ActorRef : UniqueActorReferences)
		{
			if (AActor* Actor = Cast<AActor>(ActorRef.ResolveObject())) { QueuedActors.Add(Actor); }
			else { bHasUnresolvedReferences = true; }
		}

		if (bHasUnresolvedReferences)
		{
			if (Context->GetWorld()->GetTimeSeconds() - StartTime < Settings->WaitForComponentTimeout)
			{
				PCGEX_SUBSYSTEM
				PCGExSubsystem->RegisterBeginTickAction([PCGEX_ASYNC_THIS_CAPTURE]()
				{
					PCGEX_ASYNC_THIS
					This->GatherActors();
				});

				return;
			}

			if (!Settings->bQuietTimeoutError)
			{
				for (const FSoftObjectPath& ActorRef : UniqueActorReferences)
				{
					if (AActor* Actor = Cast<AActor>(ActorRef.ResolveObject())) { continue; }
					FString Rel = TEXT("TIMEOUT : ") + ActorRef.ToString() + TEXT(" not found.");
					PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FText::FromString(Rel));
				}
			}

			PCGEX_ASYNC_RELEASE_TOKEN(SearchActorsToken)
		}
		else
		{
			StartComponentSearch();
			PCGEX_ASYNC_RELEASE_TOKEN(SearchActorsToken)
		}
	}

	void FProcessor::GatherComponents()
	{
		if (!SearchComponentsToken.IsValid()) { return; }
		if (!TaskManager->IsAvailable())
		{
			StopComponentSearch();
			return;
		}

		PerActorGatheredComponents.Reset();
		PerActorGatheredComponents.SetNum(QueuedActors.Num());

		for (int i = 0; i < QueuedActors.Num(); i++)
		{
			QueuedActors[i]->GetComponents(UPCGComponent::StaticClass(), PerActorGatheredComponents[i]);
		}

		InspectGatheredComponents();
	}

	void FProcessor::StartComponentSearch()
	{
		SearchComponentsToken = TaskManager->TryCreateToken(FName("SearchComponents"));
		if (!SearchComponentsToken.IsValid()) { return; }

		StartTime = Context->GetWorld()->GetTimeSeconds();

		PCGEX_SUBSYSTEM
		PCGExSubsystem->RegisterBeginTickAction([PCGEX_ASYNC_THIS_CAPTURE]()
		{
			PCGEX_ASYNC_THIS
			This->GatherComponents();
		});
	}

	void FProcessor::StopComponentSearch(bool bTimeout)
	{
		if (!SearchComponentsToken.IsValid()) { return; }

		if (bTimeout && !Settings->bQuietTimeoutError)
		{
			for (const AActor* Actor : QueuedActors)
			{
				FString Rel = TEXT("TIMEOUT : ") + Actor->GetName() + TEXT(" does not have ") + TemplateGraph.GetName();
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FText::FromString(Rel));
			}
		}

		PCGEX_ASYNC_RELEASE_TOKEN(SearchComponentsToken)
	}

	void FProcessor::InspectGatheredComponents()
	{
		if (!SearchComponentsToken.IsValid()) { return; }
		if (!TaskManager->IsAvailable())
		{
			StopComponentSearch();
			return;
		}

		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWaitForPCGData::InspectComponents);

		InspectionTracker->Reset(QueuedActors.Num());

		PCGEX_ASYNC_THIS_DECL
		for (int i = 0; i < QueuedActors.Num(); i++)
		{
			UE::Tasks::Launch(TEXT("ComponentInspection"), [AsyncThis, Index = i]()
			{
				PCGEX_ASYNC_THIS
				This->Inspect(Index);
			}, UE::Tasks::ETaskPriority::BackgroundLow);
		}
	}

	void FProcessor::Inspect(const int32 Index)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWaitForPCGData::Inspect);

		ON_SCOPE_EXIT { InspectionTracker->IncrementCompleted(); };

		UPCGComponent* Self = Context->GetMutableComponent();

		// Trim queued components
		TArray<UPCGComponent*> FoundComponents = PerActorGatheredComponents[Index];
		bool bHasTag = !Settings->MustHaveTag.IsNone();
		for (int i = 0; i < FoundComponents.Num(); i++)
		{
			const UPCGComponent* Candidate = FoundComponents[i];
			const UPCGGraph* CandidateGraph = Candidate->GetGraph();
			if (!CandidateGraph || !Candidate->bActivated || (Self && Candidate == Self) || (Settings->bMustMatchTemplate && CandidateGraph != TemplateGraph.Get()) || (bHasTag && !Candidate->ComponentHasTag(Settings->MustHaveTag)))
			{
				FoundComponents.RemoveAt(i);
				i--;
				continue;
			}

			if (Settings->bDoMatchGenerationTrigger)
			{
				if ((!Settings->bInvertGenerationTrigger && Candidate->GenerationTrigger != Settings->MatchGenerationTrigger) || (Settings->bInvertGenerationTrigger && Candidate->GenerationTrigger == Settings->MatchGenerationTrigger))
				{
					FoundComponents.RemoveAt(i);
					i--;
					continue;
				}
			}

			// Ensure templated outputs exist on the component
			if (!Settings->bMustMatchTemplate)
			{
				TArray<FPCGPinProperties> OutPins = CandidateGraph->GetOutputNode()->OutputPinProperties();
				for (const FName& Label : Context->RequiredLabels)
				{
					bool bFound = false;
					for (const FPCGPinProperties& Pin : OutPins)
					{
						if (Pin.Label == Label)
						{
							// TODO : Validate expected type as well
							bFound = true;
							break;
						}
					}

					// A required pin is missing
					if (!bFound)
					{
						FoundComponents.RemoveAt(i);
						i--;
						break;
					}
				}
			}
		}

		if (Settings->bWaitForMissingComponents)
		{
			if (FoundComponents.IsEmpty()) { return; } // Wait until next run
		}

		// Has not returned! Good to go.
		QueuedActors[Index] = nullptr;

		for (UPCGComponent* PCGComponent : FoundComponents) { AddValidComponent(PCGComponent); }
	}

	void FProcessor::OnInspectionComplete()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWaitForPCGData::OnInspectionComplete);

		// Inspection is complete
		// Trim actor list
		int32 WriteIndex = 0;
		for (int i = 0; i < QueuedActors.Num(); i++) { if (AActor* QueuedActor = QueuedActors[i]) { QueuedActors[WriteIndex++] = QueuedActor; } }
		QueuedActors.SetNum(WriteIndex);

		// If some actors are still enqueued, we failed to find a valid component.
		if (!QueuedActors.IsEmpty())
		{
			if (Context->GetWorld()->GetTimeSeconds() - StartTime < Settings->WaitForComponentTimeout)
			{
				PCGEX_SUBSYSTEM
				PCGExSubsystem->RegisterBeginTickAction([PCGEX_ASYNC_THIS_CAPTURE]()
				{
					PCGEX_ASYNC_THIS
					This->GatherComponents();
				});
			}
			else
			{
				StopComponentSearch(true);
			}

			return;
		}

		StopComponentSearch();
	}

	void FProcessor::AddValidComponent(UPCGComponent* InComponent)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWaitForPCGData::AddValidComponent);

		Context->EDITOR_TrackPath(InComponent);

		int32 Index = -1;

		{
			FWriteScopeLock WriteScopeLock(ValidComponentLock);
			Index = ValidComponents.Add(InComponent);
		}

		ProcessComponent(Index);
	}

	void FProcessor::WatchComponent(UPCGComponent* TargetComponent, int32 Index)
	{
		WatcherTracker->IncrementPending();

		if (!TargetComponent->IsGenerating())
		{
			StageComponentData(Index);
			return;
		}

		PCGEX_ASYNC_THIS_DECL
		PCGExMT::ExecuteOnMainThread([AsyncThis, TargetComponent, Idx = Index]()
		{
			PCGEX_ASYNC_THIS

			if (!TargetComponent->IsGenerating())
			{
				This->ScheduleComponentDataStaging(Idx);
				return;
			}

			// Make sure to not wait on cancelled generation
			TargetComponent->OnPCGGraphCancelledDelegate.AddLambda([AsyncThis, Idx](UPCGComponent* InComponent)
			{
				PCGEX_ASYNC_NESTED_THIS
				NestedThis->ValidComponents[Idx] = nullptr;
				NestedThis->WatcherTracker->IncrementCompleted();
			});

			// Wait for generated callback
			TargetComponent->OnPCGGraphGeneratedDelegate.AddLambda([AsyncThis, Idx](UPCGComponent* InComponent)
			{
				PCGEX_ASYNC_NESTED_THIS
				NestedThis->ScheduleComponentDataStaging(Idx);
			});
		});
	}

	void FProcessor::ProcessComponent(int32 Index)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWaitForPCGData::ProcessComponent);

		UPCGComponent* InComponent = ValidComponents[Index];

		switch (InComponent->GenerationTrigger)
		{
		case EPCGComponentGenerationTrigger::GenerateOnLoad: if (Settings->GenerateOnLoadAction == EPCGExGenerationTriggerAction::Ignore) { return; }
			break;
		case EPCGComponentGenerationTrigger::GenerateOnDemand: if (Settings->GenerateOnDemandAction == EPCGExGenerationTriggerAction::Ignore) { return; }
			break;
		case EPCGComponentGenerationTrigger::GenerateAtRuntime: if (Settings->GenerateAtRuntime == EPCGExRuntimeGenerationTriggerAction::Ignore) { return; }
			break;
		}

		// Ignore component getting cleaned up
		if (InComponent->IsCleaningUp()) { return; }

		// Component is actively generating.
		if (InComponent->IsGenerating())
		{
			WatchComponent(InComponent, Index);
			return;
		}

		bool bWatchComponent = false;
		bool bForce = false;

		switch (InComponent->GenerationTrigger)
		{
		case EPCGComponentGenerationTrigger::GenerateOnLoad: switch (Settings->GenerateOnLoadAction)
			{
			default: case EPCGExGenerationTriggerAction::AsIs: break;
			case EPCGExGenerationTriggerAction::ForceGenerate: bForce = true;
			case EPCGExGenerationTriggerAction::Generate: InComponent->Generate(bForce);
				bWatchComponent = true;
				break;
			}
			break;
		case EPCGComponentGenerationTrigger::GenerateOnDemand: switch (Settings->GenerateOnDemandAction)
			{
			default: case EPCGExGenerationTriggerAction::AsIs: break;
			case EPCGExGenerationTriggerAction::ForceGenerate: bForce = true;
			case EPCGExGenerationTriggerAction::Generate: InComponent->Generate(bForce);
				bWatchComponent = true;
				break;
			}
			break;
		case EPCGComponentGenerationTrigger::GenerateAtRuntime: switch (Settings->GenerateAtRuntime)
			{
			default: case EPCGExRuntimeGenerationTriggerAction::AsIs: break;
			case EPCGExRuntimeGenerationTriggerAction::RefreshFirst: if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetSubsystemForCurrentWorld())
				{
					PCGSubsystem->RefreshRuntimeGenComponent(InComponent, EPCGChangeType::GenerationGrid);
					bWatchComponent = true;
				}
				break;
			}
			break;
		}

		if (bWatchComponent)
		{
			WatchComponent(InComponent, Index);
			return;
		}

		StageComponentData(Index);
	}

	void FProcessor::ScheduleComponentDataStaging(int32 Index)
	{
		PCGEX_LAUNCH(FStageComponentDataTask, Index, SharedThis(this))
	}

	void FProcessor::StageComponentData(int32 Index)
	{
		ON_SCOPE_EXIT { WatcherTracker->IncrementCompleted(); };

		UPCGComponent* InComponent = ValidComponents[Index];
		ValidComponents[Index] = nullptr;

		const TSharedPtr<TArray<int32>>* MatchingPointsPtr = PerActorPoints.Find(InComponent->GetOwner()->GetPathName());
		if (!MatchingPointsPtr) { return; }

		const TArray<int32>& Points = *MatchingPointsPtr->Get();

		const FPCGDataCollection& GraphOutput = InComponent->GetGeneratedGraphOutput();

		if (GraphOutput.TaggedData.IsEmpty()) { return; }

		if (!Settings->bIgnoreRequiredPin)
		{
			// Ensure we have all required pins first
			for (const FName Required : Context->RequiredLabels)
			{
				if (GraphOutput.GetInputsByPin(Required).IsEmpty())
				{
					return;
				}
			}
		}

		if (Settings->bDedupeData)
		{
			TSet<FString> PointsTags;
			TargetAttributesToDataTags.Tag(PointDataFacade->GetInPoint(Points[0]), PointsTags); // Only grab first one otherwise we may end up with too many tags
			if (Settings->bCarryOverTargetTags) { PointDataFacade->Source->Tags->DumpTo(PointsTags); }

			Context->IncreaseStagedOutputReserve(GraphOutput.TaggedData.Num());

			for (const FPCGTaggedData& TaggedData : GraphOutput.TaggedData)
			{
				TSet<FString> DataTags = TaggedData.Tags;
				DataTags.Append(PointsTags);

				if (!Context->AllLabels.Contains(TaggedData.Pin))
				{
					if (Settings->bOutputRoaming)
					{
						Context->StageOutput(const_cast<UPCGData*>(TaggedData.Data.Get()), Settings->RoamingPin, PCGExData::EStaging::None, DataTags);

						// const_cast is fine here, we don't modify the data.
					}
					continue;
				}

				Context->StageOutput(const_cast<UPCGData*>(TaggedData.Data.Get()), TaggedData.Pin, PCGExData::EStaging::None, DataTags);
			}
		}
		else
		{
			Context->IncreaseStagedOutputReserve(GraphOutput.TaggedData.Num() * Points.Num());

			for (const int32 PtIndex : Points)
			{
				TSet<FString> PointsTags;
				TargetAttributesToDataTags.Tag(PointDataFacade->GetInPoint(Points[PtIndex]), PointsTags);
				if (Settings->bCarryOverTargetTags) { PointDataFacade->Source->Tags->DumpTo(PointsTags); }

				for (const FPCGTaggedData& TaggedData : GraphOutput.TaggedData)
				{
					TSet<FString> DataTags = TaggedData.Tags;
					DataTags.Append(PointsTags);

					if (!Context->AllLabels.Contains(TaggedData.Pin))
					{
						if (Settings->bOutputRoaming)
						{
							Context->StageOutput(const_cast<UPCGData*>(TaggedData.Data.Get()), Settings->RoamingPin, PCGExData::EStaging::None, DataTags);
						}
						continue;
					}

					Context->StageOutput(const_cast<UPCGData*>(TaggedData.Data.Get()), TaggedData.Pin, PCGExData::EStaging::None, DataTags);
				}
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
