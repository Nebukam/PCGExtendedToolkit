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

void UPCGExCustomGraphSettings::InitializeSettings_Implementation(const FPCGContext& InContext, bool& OutSuccess, int32& OutNodeReserve, int32& OutEdgeReserve)
{
}

void UPCGExCustomGraphSettings::AddEdge(const int64 InStartIdx, const int64 InEndIdx)
{
	if (InStartIdx == InEndIdx) { return; }
	UniqueEdges.Add(PCGEx::H64U(GetOrCreateNode(InStartIdx), GetOrCreateNode(InEndIdx)));
}

void UPCGExCustomGraphSettings::RemoveEdge(const int64 InStartIdx, const int64 InEndIdx)
{
	if (InStartIdx == InEndIdx) { return; }
	UniqueEdges.Remove(PCGEx::H64U(GetOrCreateNode(InStartIdx), GetOrCreateNode(InEndIdx)));
}

void UPCGExCustomGraphSettings::BuildGraph_Implementation(const FPCGContext& InContext, bool& OutSuccess)
{
	OutSuccess = false;
}

void UPCGExCustomGraphSettings::UpdateNodePoint_Implementation(const FPCGPoint& InPoint, const int64 InNodeIdx, const int32 InPointIndex, FPCGPoint& OutPoint) const
{
	OutPoint = InPoint;
}

void UPCGExCustomGraphBuilder::CreateGraphSettings(TSubclassOf<UPCGExCustomGraphSettings> SettingsClass, UPCGExCustomGraphSettings*& OutSettings)
{
	if (!SettingsClass || SettingsClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot instantiate an abstract class!"));
		return;
	}

	TObjectPtr<UPCGExCustomGraphSettings> NewSettings = Context->ManagedObjects->New<UPCGExCustomGraphSettings>(GetTransientPackage(), SettingsClass);
	NewSettings->SettingsIndex = GraphSettings.Add(NewSettings);
	OutSettings = NewSettings;
}

void UPCGExCustomGraphBuilder::BuildGraph_Implementation(const FPCGContext& InContext, UPCGExCustomGraphSettings* InCustomGraphSettings, bool& OutSuccess)
{
	InCustomGraphSettings->BuildGraph(InContext, OutSuccess);
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
				TUniquePtr<PCGEx::TAttributeBroadcaster<FSoftObjectPath>> ActorReferences = MakeUnique<PCGEx::TAttributeBroadcaster<FSoftObjectPath>>();
				if (!ActorReferences->Prepare(Settings->ActorReferenceAttribute, Context->CurrentIO.ToSharedRef()))
				{
					PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs don't have the specified Actor Reference attribute."));
				}

				ActorReferences->Grab();
				TSet<AActor*> UniqueActors;

				for (FSoftObjectPath Path : ActorReferences->Values)
				{
					if (AActor* SourceActor = Cast<AActor>(Path.ResolveObject())) { UniqueActors.Add(SourceActor); }
				}

				Context->Builder->InputActors.Reserve(UniqueActors.Num());
				Context->Builder->InputActors.Append(UniqueActors.Array());
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
			TSharedPtr<PCGExData::FPointIO> NodeIO = Context->MainPoints->Emplace_GetRef();
			NodeIO->IOIndex = GraphSettings->SettingsIndex;
			Context->GetAsyncManager()->Start<PCGExBuildCustomGraph::FBuildGraph>(GraphSettings->SettingsIndex, NodeIO, GraphSettings);
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

		bool bInitSuccess = false;
		int32 NodeReserveNum = 0;
		int32 EdgeReserveNum = 0;
		GraphSettings->InitializeSettings(*Context, bInitSuccess, NodeReserveNum, EdgeReserveNum);

		if (!bInitSuccess)
		{
			if (!Settings->bMuteUnprocessedSettingsWarning)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("A graph builder settings has less than 2 max nodes and won't be processed."));
			}

			PointIO->InitializeOutput(Context, PCGExData::EInit::NoOutput);
			GraphSettings->GraphBuilder->bCompiledSuccessfully = false;
			return false;
		}

		if (NodeReserveNum > 0)
		{
			GraphSettings->Idx.Reserve(NodeReserveNum);
			GraphSettings->IdxMap.Reserve(NodeReserveNum);
		}

		if (EdgeReserveNum > 0)
		{
			GraphSettings->UniqueEdges.Reserve(EdgeReserveNum);
		}
		else if (NodeReserveNum > 0)
		{
			GraphSettings->UniqueEdges.Reserve(NodeReserveNum * 3); // Wild guess
		}

		bool bSuccessfulBuild = false;
		Builder->BuildGraph(*Context, GraphSettings, bSuccessfulBuild);

		if (!bSuccessfulBuild)
		{
			GraphSettings->GraphBuilder->bCompiledSuccessfully = false;
			return false;
		}

		PointIO->GetOut()->GetMutablePoints().SetNum(GraphSettings->Idx.Num());

		TSharedPtr<PCGExData::FFacade> NodeDataFacade = MakeShared<PCGExData::FFacade>(PointIO.ToSharedRef());
		const TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(NodeDataFacade.ToSharedRef(), &Settings->GraphBuilderDetails);
		GraphBuilder->OutputNodeIndices = MakeShared<TArray<int32>>();

		GraphSettings->GraphBuilder = GraphBuilder;

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
		InitNodesGroup->OnIterationRangeStartCallback = [WeakIO, CustomGraphSettings](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
		{
			const TSharedPtr<PCGExData::FPointIO> IO = WeakIO.Pin();
			if (!IO) { return; }

			const int32 MaxIndex = StartIndex + Count;
			TArray<FPCGPoint>& MutablePoints = IO->GetOut()->GetMutablePoints();
			for (int i = StartIndex; i < MaxIndex; i++)
			{
				FPCGPoint& Point = MutablePoints[i];
				CustomGraphSettings->UpdateNodePoint(Point, CustomGraphSettings->Idx[i], i, Point);
			}
		};

		InitNodesGroup->StartRangePrepareOnly(CustomGraphSettings->Idx.Num(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
