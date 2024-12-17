// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExMeshToClusters.h"


#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Geometry/PCGExGeoMesh.h"
#include "Graph/PCGExCluster.h"
#include "Graph/Data/PCGExClusterData.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE MeshToClusters

namespace PCGExGeoTask
{
	class FLloydRelax3;
}

PCGExData::EIOInit UPCGExMeshToClustersSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

TArray<FPCGPinProperties> UPCGExMeshToClustersSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(MeshToClusters)

bool FPCGExMeshToClustersElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(MeshToClusters)
	PCGEX_EXECUTION_CHECK

	if (Context->MainPoints->Pairs.Num() < 1)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing targets."));
		return false;
	}

	Context->TargetsDataFacade = MakeShared<PCGExData::FFacade>(Context->MainPoints->Pairs[0].ToSharedRef());

	PCGEX_FWD(GraphBuilderDetails)

	PCGEX_FWD(TransformDetails)
	if (!Context->TransformDetails.Init(Context, Context->TargetsDataFacade.ToSharedRef())) { return false; }

	if (Settings->StaticMeshInput == EPCGExInputValueType::Attribute)
	{
		PCGEX_VALIDATE_NAME(Settings->StaticMeshAttribute)
	}

	const TSharedPtr<PCGExData::FPointIO> Targets = Context->MainPoints->Pairs[0];
	Context->MeshIdx.SetNum(Targets->GetNum());

	Context->StaticMeshMap = MakeShared<PCGExGeo::FGeoStaticMeshMap>();
	Context->StaticMeshMap->DesiredTriangulationType = Settings->GraphOutputType;

	Context->RootVtx = MakeShared<PCGExData::FPointIOCollection>(Context); // Make this pinless

	Context->VtxChildCollection = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->VtxChildCollection->OutputPin = Settings->GetMainOutputPin();

	Context->EdgeChildCollection = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->EdgeChildCollection->OutputPin = PCGExGraph::OutputEdgesLabel;

	return true;
}

bool FPCGExMeshToClustersElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMeshToClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(MeshToClusters)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->AdvancePointsIO();
		if (Settings->StaticMeshInput == EPCGExInputValueType::Constant)
		{
			if (!Settings->StaticMeshConstant.ToSoftObjectPath().IsValid())
			{
				PCGE_LOG(Error, GraphAndLog, FTEXT("Invalid static mesh constant"));
				return false;
			}

			const int32 Idx = Context->StaticMeshMap->Find(Settings->StaticMeshConstant.ToSoftObjectPath());

			if (Idx == -1)
			{
				PCGE_LOG(Error, GraphAndLog, FTEXT("Static mesh constant could not be loaded."));
				return false;
			}

			for (int32& Index : Context->MeshIdx) { Index = Idx; }
		}
		else
		{
			FPCGAttributePropertyInputSelector Selector = FPCGAttributePropertyInputSelector();
			Selector.SetAttributeName(Settings->StaticMeshAttribute);

			const TUniquePtr<PCGEx::TAttributeBroadcaster<FSoftObjectPath>> PathGetter = MakeUnique<PCGEx::TAttributeBroadcaster<FSoftObjectPath>>();
			if (!PathGetter->Prepare(Selector, Context->MainPoints->Pairs[0].ToSharedRef()))
			{
				PCGE_LOG(Error, GraphAndLog, FTEXT("Static mesh attribute does not exists on targets."));
				return false;
			}

			const TArray<FPCGPoint>& TargetPoints = Context->CurrentIO->GetIn()->GetPoints();
			for (int i = 0; i < TargetPoints.Num(); i++)
			{
				FSoftObjectPath Path = PathGetter->SoftGet(i, TargetPoints[i], FSoftObjectPath());

				if (!Path.IsValid())
				{
					if (!Settings->bIgnoreMeshWarnings) { PCGE_LOG(Warning, GraphAndLog, FTEXT("Some targets could not have their mesh loaded.")); }
					Context->MeshIdx[i] = -1;
					continue;
				}

				if (Settings->AttributeHandling == EPCGExMeshAttributeHandling::StaticMeshSoftPath)
				{
					const int32 Idx = Context->StaticMeshMap->Find(Path);

					if (Idx == -1)
					{
						if (!Settings->bIgnoreMeshWarnings) { PCGE_LOG(Warning, GraphAndLog, FTEXT("Some targets could not have their mesh loaded.")); }
						Context->MeshIdx[i] = -1;
					}
					else
					{
						Context->MeshIdx[i] = Idx;
					}
				}
				else
				{
					TArray<UStaticMeshComponent*> SMComponents;
					if (const AActor* SourceActor = Cast<AActor>(Path.ResolveObject()))
					{
						TArray<UActorComponent*> Components;
						SourceActor->GetComponents(Components);

						for (UActorComponent* Component : Components)
						{
							if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
							{
								SMComponents.Add(StaticMeshComponent);
							}
						}
					}

					if (SMComponents.IsEmpty())
					{
						Context->MeshIdx[i] = -1;
					}
					else
					{
						const int32 Idx = Context->StaticMeshMap->Find(TSoftObjectPtr<UStaticMesh>(SMComponents[0]->GetStaticMesh()).ToSoftObjectPath());
						if (Idx == -1)
						{
							if (!Settings->bIgnoreMeshWarnings) { PCGE_LOG(Warning, GraphAndLog, FTEXT("Some actors have invalid SMCs.")); }
							Context->MeshIdx[i] = -1;
						}
						else
						{
							Context->MeshIdx[i] = Idx;
						}
					}
				}
			}
		}

		const int32 GSMNums = Context->StaticMeshMap->GSMs.Num();
		Context->GraphBuilders.SetNum(GSMNums);
		for (int i = 0; i < GSMNums; i++) { Context->GraphBuilders[i] = nullptr; }

		const TSharedPtr<PCGExMT::FTaskManager> AsyncManager = Context->GetAsyncManager();
		for (int i = 0; i < Context->StaticMeshMap->GSMs.Num(); i++)
		{
			PCGEX_LAUNCH(PCGExMeshToCluster::FExtractMeshAndBuildGraph, i, Context->StaticMeshMap->GSMs[i])
		}

		// Preload all & build local graphs to copy to points later on			
		Context->SetAsyncState(PCGExGeo::State_ExtractingMesh);
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExGeo::State_ExtractingMesh)
	{
		const TSharedPtr<PCGExMT::FTaskManager> AsyncManager = Context->GetAsyncManager();
		const TArray<FPCGPoint>& Targets = Context->CurrentIO->GetIn()->GetPoints();
		for (int i = 0; i < Targets.Num(); i++)
		{
			const int32 MeshIdx = Context->MeshIdx[i];

			if (MeshIdx == -1) { continue; }

			PCGEX_LAUNCH(PCGExGraphTask::FCopyGraphToPoint, i, Context->CurrentIO, Context->GraphBuilders[MeshIdx], Context->VtxChildCollection, Context->EdgeChildCollection, &Context->TransformDetails)
		}

		Context->SetAsyncState(PCGExGraph::State_WritingClusters);
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExGraph::State_WritingClusters)
	{
		Context->VtxChildCollection->StageOutputs();
		Context->EdgeChildCollection->StageOutputs();

		Context->Done();
	}

	return Context->TryComplete();
}

namespace PCGExMeshToCluster
{
	void FExtractMeshAndBuildGraph::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		FPCGExMeshToClustersContext* Context = AsyncManager->GetContext<FPCGExMeshToClustersContext>();
		PCGEX_SETTINGS(MeshToClusters)

		switch (Mesh->DesiredTriangulationType)
		{
		default: ;
		case EPCGExTriangulationType::Raw:
			Mesh->ExtractMeshSynchronous();
			break;
		case EPCGExTriangulationType::Dual:
			Mesh->TriangulateMeshSynchronous();
			Mesh->MakeDual();
			break;
		case EPCGExTriangulationType::Hollow:
			Mesh->TriangulateMeshSynchronous();
			Mesh->MakeHollowDual();
			break;
		}

		const TSharedPtr<PCGExData::FPointIO> RootVtx = Context->RootVtx->Emplace_GetRef<UPCGExClusterNodesData>();
		if (!RootVtx) { return; }

		RootVtx->IOIndex = TaskIndex;
		TArray<FPCGPoint>& VtxPoints = RootVtx->GetOut()->GetMutablePoints();
		VtxPoints.SetNum(Mesh->Vertices.Num());

		PCGEX_MAKE_SHARED(RootVtxFacade, PCGExData::FFacade, RootVtx.ToSharedRef())

		PCGEX_MAKE_SHARED(GraphBuilder, PCGExGraph::FGraphBuilder, RootVtxFacade.ToSharedRef(), &Context->GraphBuilderDetails)
		Context->GraphBuilders[TaskIndex] = GraphBuilder;

		for (int i = 0; i < VtxPoints.Num(); i++)
		{
			FPCGPoint& NewVtx = VtxPoints[i];
			NewVtx.Transform.SetLocation(Mesh->Vertices[i]);
		}

		GraphBuilder->Graph->InsertEdges(Mesh->Edges, -1);
		GraphBuilder->CompileAsync(Context->GetAsyncManager(), true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
