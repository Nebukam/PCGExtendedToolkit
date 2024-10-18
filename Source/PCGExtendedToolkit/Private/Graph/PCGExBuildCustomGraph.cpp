// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildCustomGraph.h"


#include "Graph/PCGExGraph.h"
#include "Graph/Data/PCGExClusterData.h"
#include "Graph/PCGExUnionHelpers.h"
#include "Graph/Probes/PCGExProbeOperation.h"

#define LOCTEXT_NAMESPACE "PCGExBuildCustomGraphElement"
#define PCGEX_NAMESPACE BuildCustomGraph

void UPCGExCustomGraphBuilder::InitializeWithContext_Implementation(const FPCGContext& InContext, bool& OutSuccess)
{
	OutSuccess = false;
}

void UPCGExCustomGraphSettings::AddEdge(const int32 InStartIndex, const int32 InEndIndex)
{
	if (InStartIndex < 0 || InEndIndex < 0 || InStartIndex > MaxIndex || InEndIndex > MaxIndex) { return; }
	UniqueEdges.Add(PCGEx::H64U(InStartIndex, InEndIndex));
}

void UPCGExCustomGraphSettings::UpdateNodePoint_Implementation(const int32 InNodeIndex, const FPCGPoint& InPoint, FPCGPoint& OutPoint) const
{
	OutPoint = InPoint;
}

UPCGExCustomGraphSettings* UPCGExCustomGraphBuilder::CreateGraphSettings(TSubclassOf<UPCGExCustomGraphSettings> SettingsClass, int32 InMaxNumNodes)
{
	TObjectPtr<UPCGExCustomGraphSettings> NewSettings = Context->ManagedObjects->New<UPCGExCustomGraphSettings>(GetTransientPackage(), SettingsClass);
	NewSettings->Index = GraphSettings.Add(NewSettings);
	NewSettings->MaxNumNodes = InMaxNumNodes;
	NewSettings->MaxIndex = InMaxNumNodes - 1; // Might need to update prior to processing
	return NewSettings;
}

void UPCGExCustomGraphBuilder::BuildGraph_Implementation(const FPCGContext& InContext, UPCGExCustomGraphSettings* InCustomGraphSettings, bool& OutSuccess)
{
	OutSuccess = false;
}

void UPCGExCustomGraphBuilder::UpdateNodePoint_Implementation(const UPCGExCustomGraphSettings* InCustomGraphSettings, const int32 InNodeIndex, const FPCGPoint& InPoint, FPCGPoint& OutPoint) const
{
	InCustomGraphSettings->UpdateNodePoint(InNodeIndex, InPoint, OutPoint);
}

TArray<FPCGPinProperties> UPCGExBuildCustomGraphSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExBuildCustomGraphSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(BuildCustomGraph)

bool FPCGExBuildCustomGraphElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildCustomGraph)

	if (!Settings->Builder)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No builder selected."));
		return false;
	}

	PCGEX_OPERATION_BIND(Builder, UPCGExCustomGraphBuilder)

	if (Settings->Mode == EPCGExCustomGraphActorSourceMode::ActorReferences)
	{
		PCGEX_VALIDATE_NAME(Settings->ActorReferenceAttribute)
	}

	return true;
}

bool FPCGExBuildCustomGraphElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildCustomGraphElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildCustomGraph)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (Settings->Mode == EPCGExCustomGraphActorSourceMode::Owner)
		{
			Context->Builder->InputActors.Add(Context->SourceComponent->GetOwner());
		}
		else
		{
			while (Context->AdvancePointsIO())
			{
				TUniquePtr<PCGEx::TAttributeBroadcaster<FString>> ActorReferences = MakeUnique<PCGEx::TAttributeBroadcaster<FString>>();
				if (!ActorReferences->Prepare(Settings->ActorReferenceAttribute, Context->CurrentIO.ToSharedRef()))
				{
					PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs don't have the specified Actor Reference attribute."));
				}

				ActorReferences->Grab();

				for (FString Path : ActorReferences->Values)
				{
					if (UObject* FoundObject = FindObject<AActor>(nullptr, *Path))
					{
						if (AActor* SourceActor = Cast<AActor>(FoundObject))
						{
							Context->Builder->InputActors.AddUnique(SourceActor);
						}
					}
				}
			}
		}

		// Init builder now that we have resolved actor references.

		bool bSuccessfulInit = false;
		Context->Builder->InitializeWithContext(*Context, bSuccessfulInit);
		if (!bSuccessfulInit)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Builder returned failed initialization."));
			return true;
		}

		// Prepare graph builders
		if (Context->Builder->GraphSettings.IsEmpty())
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Builder generated no graph settings."));
			return true;
		}

		Context->SetAsyncState(PCGExGraph::State_WritingClusters);

		for (UPCGExCustomGraphSettings* GraphSettings : Context->Builder->GraphSettings)
		{
			if (GraphSettings->MaxNumNodes < 2)
			{
				if (!Settings->bMuteUnprocessedSettingsWarning)
				{
					PCGE_LOG(Warning, GraphAndLog, FTEXT("A graph builder settings has less than 2 max nodes and won't be processed."));
				}
				continue;
			}

			TSharedPtr<PCGExData::FPointIO> NodeIO = Context->MainPoints->Emplace_GetRef();
			NodeIO->IOIndex = GraphSettings->Index;
			GraphSettings->MaxIndex = GraphSettings->MaxNumNodes - 1;
			Context->GetAsyncManager()->Start<PCGExBuildCustomGraph::FBuildGraph>(GraphSettings->Index, NodeIO, GraphSettings);
		}

		return false;
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExGraph::State_WritingClusters)
	{
		for (UPCGExCustomGraphSettings* GraphSettings : Context->Builder->GraphSettings)
		{
			if (!GraphSettings->GraphBuilder) { continue; }
			if (GraphSettings->GraphBuilder->bCompiledSuccessfully)
			{
				GraphSettings->GraphBuilder->StageEdgesOutputs();
			}
			else
			{
				// Invalidate node IO
				GraphSettings->GraphBuilder->NodeDataFacade->Source->InitializeOutput(Context, PCGExData::EInit::NoOutput);
			}
		}

		Context->MainPoints->StageOutputs();
		Context->Done();
	}

	return Context->TryComplete();
}

namespace PCGExBuildCustomGraph
{
	bool FBuildGraph::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		FPCGExBuildCustomGraphContext* Context = AsyncManager->GetContext<FPCGExBuildCustomGraphContext>();
		PCGEX_SETTINGS(BuildCustomGraph)

		UPCGExCustomGraphBuilder* Builder = Context->Builder;

		PointIO->GetOut()->GetMutablePoints().SetNum(GraphSettings->MaxNumNodes);

		TSharedPtr<PCGExData::FFacade> NodeDataFacade = MakeShared<PCGExData::FFacade>(PointIO.ToSharedRef());
		const TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(NodeDataFacade.ToSharedRef(), &Settings->GraphBuilderDetails);
		GraphSettings->GraphBuilder = GraphBuilder;

		bool bSuccessfulBuild = false;
		Builder->BuildGraph(*Context, GraphSettings, bSuccessfulBuild);
		if (!bSuccessfulBuild)
		{
			GraphSettings->GraphBuilder->bCompiledSuccessfully = false;
			return false;
		}

		GraphBuilder->Graph->InsertEdges(GraphSettings->UniqueEdges, -1);

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, InitNodesGroup)

		TWeakPtr<PCGExData::FPointIO> WeakIO = PointIO;
		TWeakPtr<PCGExGraph::FGraphBuilder> WeakGraphBuilder = GraphBuilder;

		InitNodesGroup->OnCompleteCallback = [WeakGraphBuilder, AsyncManager]()
		{
			const TSharedPtr<PCGExGraph::FGraphBuilder> GBuilder = WeakGraphBuilder.Pin();
			if (!GBuilder) { return; }

			GBuilder->CompileAsync(AsyncManager, true);
		};

		UPCGExCustomGraphSettings* CustomGraphSettings = GraphSettings;
		InitNodesGroup->OnIterationRangeStartCallback = [WeakIO, Builder, CustomGraphSettings](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
		{
			const TSharedPtr<PCGExData::FPointIO> IO = WeakIO.Pin();
			if (!IO) { return; }

			const int32 MaxIndex = StartIndex + Count;
			TArray<FPCGPoint>& MutablePoints = IO->GetOut()->GetMutablePoints();
			for (int i = StartIndex; i < MaxIndex; i++)
			{
				FPCGPoint& Point = MutablePoints[i];
				Builder->UpdateNodePoint(CustomGraphSettings, i, Point, Point);
			}
		};

		InitNodesGroup->StartRangePrepareOnly(GraphSettings->MaxNumNodes, 256);

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
