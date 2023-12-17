// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExConsolidateEdgeIslands.h"

#define LOCTEXT_NAMESPACE "PCGExConsolidateEdgeIslands"
#define PCGEX_NAMESPACE ConsolidateEdgeIslands

UPCGExConsolidateEdgeIslandsSettings::UPCGExConsolidateEdgeIslandsSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExData::EInit UPCGExConsolidateEdgeIslandsSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

bool UPCGExConsolidateEdgeIslandsSettings::GetCacheAllMeshes() const { return true; }

FPCGElementPtr UPCGExConsolidateEdgeIslandsSettings::CreateElement() const { return MakeShared<FPCGExConsolidateEdgeIslandsElement>(); }

FPCGExConsolidateEdgeIslandsContext::~FPCGExConsolidateEdgeIslandsContext()
{
	PCGEX_TERMINATE_ASYNC

	ConsolidatedEdges = nullptr;

	VisitedMeshes.Empty();
}


PCGEX_INITIALIZE_CONTEXT(ConsolidateEdgeIslands)

bool FPCGExConsolidateEdgeIslandsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ConsolidateEdgeIslands)

	return true;
}

bool FPCGExConsolidateEdgeIslandsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExConsolidateEdgeIslandsElement::Execute);

	PCGEX_CONTEXT(ConsolidateEdgeIslands)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		Context->VisitedMeshes.Empty();

		if (!Context->AdvanceAndBindPointsIO()) { Context->Done(); }
		else
		{
			if (!Context->BoundEdges->IsValid())
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no associated edges."));
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
			}
			else
			{
				Context->ConsolidatedEdges = &Context->Edges->Emplace_GetRef(*Context->BoundEdges->Values[0], PCGExData::EInit::NewOutput);
				TArray<FPCGPoint>& MutablePoints = Context->ConsolidatedEdges->GetOut()->GetMutablePoints();

				if (Context->BoundEdges->Values.Num() == 1)
				{
					// No islands to consolidate, just dump existing points
					MutablePoints.Append(Context->BoundEdges->Values[0]->GetIn()->GetPoints());
					Context->SetState(PCGExMT::State_ReadyForNextPoints);
				}
				else
				{
					int32 TotalEdgeCount = 0;
					for (const PCGExData::FPointIO* BoundEdges : Context->BoundEdges->Values) { TotalEdgeCount += BoundEdges->GetNum(); }
					MutablePoints.Reserve(TotalEdgeCount + (Context->BoundEdges->Values.Num() - 1));

					// Dump
					for (const PCGExData::FPointIO* BoundEdges : Context->BoundEdges->Values)
					{
						MutablePoints.Append(Context->BoundEdges->Values[0]->GetIn()->GetPoints());
					}

					Context->SetState(PCGExGraph::State_ReadyForNextEdges);
				}
			}
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		while (Context->AdvanceEdges()) { /* Batch-build all meshes since bCacheAllMeshes == true */ }
		Context->SetState(PCGExGraph::State_ProcessingEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		auto BridgeIslands = [&](const int32 MeshIndex)
		{
			PCGExMesh::FMesh* CurrentMesh = Context->Meshes[MeshIndex];
			Context->VisitedMeshes.Add(CurrentMesh); // As to not connect to self or already connected

			PCGExMesh::FMesh* ClosestMesh = nullptr;
			double Distance = TNumericLimits<double>::Max();
			for (PCGExMesh::FMesh* OtherMesh : Context->Meshes)
			{
				if (Context->VisitedMeshes.Contains(OtherMesh)) { continue; }
				double Dist = FVector::DistSquared(CurrentMesh->Bounds.GetCenter(), OtherMesh->Bounds.GetCenter());
				if (!ClosestMesh || Dist < Distance)
				{
					ClosestMesh = OtherMesh;
					Distance = Dist;
				}
			}

			Context->GetAsyncManager()->StartSync<FBridgeMeshesTask>(MeshIndex, Context->ConsolidatedEdges, Context->Meshes.IndexOfByKey(ClosestMesh));
		};

		if (Context->Process(BridgeIslands, Context->Meshes.Num() - 1, true))
		{
			Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
		}
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (Context->IsAsyncWorkComplete()) { Context->SetState(PCGExMT::State_ReadyForNextPoints); }
	}

	if (Context->IsDone()) { Context->OutputPointsAndEdges(); }

	return Context->IsDone();
}

bool FBridgeMeshesTask::ExecuteTask()
{
	FPCGExConsolidateEdgeIslandsContext* Context = Manager->GetContext<FPCGExConsolidateEdgeIslandsContext>();
	//PCGEX_ASYNC_CHECKPOINT

	const TArray<PCGExMesh::FVertex>& CurrentMeshVertices = Context->Meshes[TaskIndex]->Vertices;
	const TArray<PCGExMesh::FVertex>& OtherMeshVertices = Context->Meshes[OtherMeshIndex]->Vertices;

	int32 IndexA = -1;
	int32 IndexB = -1;

	double Distance = TNumericLimits<double>::Max();

	//Brute force find closest points
	for (int i = 0; i < CurrentMeshVertices.Num(); i++)
	{
		const PCGExMesh::FVertex& CurrentVtx = CurrentMeshVertices[i];

		for (int j = 0; j < OtherMeshVertices.Num(); j++)
		{
			double Dist = FVector::DistSquared(CurrentVtx.Position, OtherMeshVertices[j].Position);
			if (Dist < Distance)
			{
				IndexA = i;
				IndexB = j;
				Distance = Dist;
			}
		}
	}

	if (IndexA == -1) { IndexA = 0; }
	if (IndexB == -1) { IndexB = 0; }

	int32 EdgeIndex = -1;
	const FPCGPoint& Bridge = Context->ConsolidatedEdges->NewPoint(EdgeIndex);
	const PCGMetadataEntryKey BridgeKey = Bridge.MetadataEntry;
	UPCGMetadata* OutMetadataData = Context->ConsolidatedEdges->GetOut()->Metadata;
	OutMetadataData->FindOrCreateAttribute<int32>(PCGExGraph::EdgeStartAttributeName)->SetValue(BridgeKey, IndexA);
	OutMetadataData->FindOrCreateAttribute<int32>(PCGExGraph::EdgeEndAttributeName)->SetValue(BridgeKey, IndexB);

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
