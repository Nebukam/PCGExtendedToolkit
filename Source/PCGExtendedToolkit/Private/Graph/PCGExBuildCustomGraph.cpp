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

void UPCGExCustomGraphSettings::InitPointAttributes_Implementation(const FPCGContext& InContext, bool& OutSuccess)
{
	OutSuccess = true;
}

void UPCGExCustomGraphSettings::BuildGraph_Implementation(const FPCGContext& InContext, bool& OutSuccess)
{
	OutSuccess = false;
}

void UPCGExCustomGraphSettings::UpdateNodePoint_Implementation(const FPCGPoint& InPoint, const int64 InNodeIdx, const int32 InPointIndex, FPCGPoint& OutPoint)
{
	OutPoint = InPoint;
}

// Convenient macro to avoid duplicating a lot of code with all our supported types.
#define PCGEX_FOREACH_ATTR_TYPE(MACRO) \
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

#pragma region Node Attributes

#define PCGEX_SET_ATT_IMPL(_NAME, _TYPE)\
bool UPCGExCustomGraphSettings::InitNode##_NAME(const FName& InAttributeName, const _TYPE& InValue){\
TSharedPtr<PCGExData::TBuffer<_TYPE>> Buffer = VtxBuffers->GetBuffer<_TYPE>(InAttributeName, InValue);\
return Buffer ? true : false;}
PCGEX_FOREACH_ATTR_TYPE(PCGEX_SET_ATT_IMPL)
#undef PCGEX_SET_ATT_IMPL

bool UPCGExCustomGraphSettings::InitNodeSoftObjectPath(const FName& InAttributeName, const FSoftObjectPath& InValue)
{
#if PCGEX_ENGINE_VERSION <= 503
	return InitNodeString(InAttributeName, InValue.ToString());
#else
	TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> Buffer = VtxBuffers->GetBuffer<FSoftObjectPath>(InAttributeName, InValue);
	return Buffer ? true : false;
#endif
}

bool UPCGExCustomGraphSettings::InitNodeSoftClassPath(const FName& InAttributeName, const FSoftClassPath& InValue)
{
#if PCGEX_ENGINE_VERSION <= 503
	return InitNodeString(InAttributeName, InValue.ToString());
#else
	TSharedPtr<PCGExData::TBuffer<FSoftClassPath>> Buffer = VtxBuffers->GetBuffer<FSoftClassPath>(InAttributeName, InValue);
	return Buffer ? true : false;
#endif
}

#define PCGEX_SET_ATT_IMPL(_NAME, _TYPE)\
bool UPCGExCustomGraphSettings::SetNode##_NAME(const FName& InAttributeName, const int64 InNodeID, const _TYPE& InValue){\
return VtxBuffers->SetValue<_TYPE>(InAttributeName, IdxMap[InNodeID], InValue);}
PCGEX_FOREACH_ATTR_TYPE(PCGEX_SET_ATT_IMPL)
#undef PCGEX_SET_ATT_IMPL

bool UPCGExCustomGraphSettings::SetNodeSoftObjectPath(const FName& InAttributeName, const int64 InNodeID, const FSoftObjectPath& InValue)
{
#if PCGEX_ENGINE_VERSION <= 503
	return VtxBuffers->SetValue<FString>(InAttributeName, IdxMap[InNodeID], InValue.ToString());
#else
	return VtxBuffers->SetValue<FSoftObjectPath>(InAttributeName, IdxMap[InNodeID], InValue);
#endif
}

bool UPCGExCustomGraphSettings::SetNodeSoftClassPath(const FName& InAttributeName, const int64 InNodeID, const FSoftClassPath& InValue)
{
#if PCGEX_ENGINE_VERSION <= 503
	return VtxBuffers->SetValue<FString>(InAttributeName, IdxMap[InNodeID], InValue.ToString());
#else
	return VtxBuffers->SetValue<FSoftClassPath>(InAttributeName, IdxMap[InNodeID], InValue);
#endif
}

#pragma endregion

#undef PCGEX_FOREACH_ATTR_TYPE


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

TArray<FPCGPinProperties> UPCGExBuildCustomGraphSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExBuildCustomGraph::SourceOverridesBuilder)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExBuildCustomGraphSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	return PinProperties;
}

PCGExData::EIOInit UPCGExBuildCustomGraphSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NoOutput; }

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

	PCGEX_OPERATION_BIND(Builder, UPCGExCustomGraphBuilder, PCGExBuildCustomGraph::SourceOverridesBuilder)

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
				GraphSettings->GraphBuilder->NodeDataFacade->Source->InitializeOutput(PCGExData::EIOInit::NoOutput);
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

			PointIO->InitializeOutput(PCGExData::EIOInit::NoOutput);
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
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("A graph builder 'BuildGraph' returned false."));
			GraphSettings->GraphBuilder->bCompiledSuccessfully = false;
			return false;
		}

		PointIO->GetOut()->GetMutablePoints().SetNum(GraphSettings->Idx.Num());

		TSharedPtr<PCGExData::FFacade> NodeDataFacade = MakeShared<PCGExData::FFacade>(PointIO.ToSharedRef());
		const TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(NodeDataFacade.ToSharedRef(), &Settings->GraphBuilderDetails);
		GraphBuilder->OutputNodeIndices = MakeShared<TArray<int32>>();

		GraphSettings->VtxBuffers = MakeShared<PCGExData::FBufferHelper>(NodeDataFacade.ToSharedRef());
		GraphSettings->GraphBuilder = GraphBuilder;

		GraphBuilder->Graph->InsertEdges(GraphSettings->UniqueEdges, -1);

		bool bSuccessfulAttrInit = false;
		GraphSettings->InitPointAttributes(*Context, bSuccessfulAttrInit);

		if (!bSuccessfulAttrInit)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("A graph builder 'InitPointAttributes' returned false."));
			GraphSettings->GraphBuilder->bCompiledSuccessfully = false;
			return false;
		}

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
		InitNodesGroup->OnSubLoopStartCallback = [WeakIO, CustomGraphSettings](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
		{
			const TSharedPtr<PCGExData::FPointIO> IO = WeakIO.Pin();
			if (!IO) { return; }

			const int32 MaxIndex = StartIndex + Count;
			TArray<FPCGPoint>& MutablePoints = IO->GetOut()->GetMutablePoints();
			IO->GetOutKeys(true); // Generate out keys

			for (int i = StartIndex; i < MaxIndex; i++)
			{
				FPCGPoint& Point = MutablePoints[i];
				CustomGraphSettings->UpdateNodePoint(Point, CustomGraphSettings->Idx[i], i, Point);
			}
		};

		InitNodesGroup->StartSubLoops(CustomGraphSettings->Idx.Num(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
