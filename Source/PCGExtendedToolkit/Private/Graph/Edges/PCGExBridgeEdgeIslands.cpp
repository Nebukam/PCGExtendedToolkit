// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExBridgeEdgeIslands.h"

#include "Data/PCGExPointIOMerger.h"
#include "Geometry/PCGExGeoDelaunay.h"

#define LOCTEXT_NAMESPACE "PCGExBridgeEdgeIslands"
#define PCGEX_NAMESPACE BridgeEdgeIslands

UPCGExBridgeEdgeIslandsSettings::UPCGExBridgeEdgeIslandsSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExData::EInit UPCGExBridgeEdgeIslandsSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

bool UPCGExBridgeEdgeIslandsSettings::GetCacheAllMeshes() const { return true; }

PCGEX_INITIALIZE_ELEMENT(BridgeEdgeIslands)

FPCGExBridgeEdgeIslandsContext::~FPCGExBridgeEdgeIslandsContext()
{
	PCGEX_TERMINATE_ASYNC

	ConsolidatedEdges = nullptr;

	VisitedMeshes.Empty();
}


bool FPCGExBridgeEdgeIslandsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BridgeEdgeIslands)

	PCGEX_FWD(BridgeMethod)

	return true;
}

bool FPCGExBridgeEdgeIslandsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBridgeEdgeIslandsElement::Execute);

	PCGEX_CONTEXT(BridgeEdgeIslands)

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
					FPCGExPointIOMerger* Merger = new FPCGExPointIOMerger(*Context->ConsolidatedEdges);
					Merger->Append(Context->BoundEdges->Values);
					Merger->DoMerge();
					Context->SetState(PCGExGraph::State_ReadyForNextEdges);
				}
			}
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		while (Context->AdvanceEdges())
		{
			/* Batch-build all meshes since bCacheAllMeshes == true */
			if (Context->CurrentMesh->HasInvalidEdges())
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input edges are invalid. This will highly likely cause unexpected results."));
			}
		}
		Context->SetState(PCGExGraph::State_ProcessingEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		auto BridgeIslands = [&](const int32 MeshIndex)
		{
			PCGExMesh::FMesh* CurrentMesh = Context->Meshes[MeshIndex];

			if(Context->BridgeMethod == EPCGExBridgeIslandMethod::Delaunay)
			{
				PCGExGeo::TDelaunayTriangulation3* Delaunay = new PCGExGeo::TDelaunayTriangulation3();
				TArray<FPCGPoint> Points;
				Points.SetNum(Context->Meshes.Num());
				for(int i = 0; i < Points.Num(); i++){Points[i].Transform.SetLocation(Context->Meshes[i]->Bounds.GetCenter());}
				if(Delaunay->PrepareFrom(Points))
				{
					Delaunay->Generate();
					
					TSet<uint64> UniqueEdges;
					UniqueEdges.Reserve(Delaunay->Cells.Num() * 3);
					for (const PCGExGeo::TDelaunayCell<4>* Cell : Delaunay->Cells)
					{
						for (int i = 0; i < 4; i++)
						{
							const int32 A = Cell->Simplex->Vertices[i]->Id;
							for (int j = i + 1; j < 4; j++)
							{
								const int32 B = Cell->Simplex->Vertices[j]->Id;
								const uint64 Hash = PCGExGraph::GetUnsignedHash64(A, B);
								if (!UniqueEdges.Contains(Hash))
								{
									Context->GetAsyncManager()->Start<FBridgeMeshesTask>(A, Context->ConsolidatedEdges, B);
									UniqueEdges.Add(Hash);
								}
							}
						}
					}

					UniqueEdges.Empty();
				}
				
				PCGEX_DELETE(Delaunay)
			}
			else if (Context->BridgeMethod == EPCGExBridgeIslandMethod::LeastEdges)
			{
				Context->VisitedMeshes.Add(CurrentMesh); // As to not connect to self or already connected

				PCGExMesh::FMesh* ClosestMesh = nullptr;
				double Distance = TNumericLimits<double>::Max();
				for (PCGExMesh::FMesh* OtherMesh : Context->Meshes)
				{
					if (Context->VisitedMeshes.Contains(OtherMesh)) { continue; }
					const double Dist = FVector::DistSquared(CurrentMesh->Bounds.GetCenter(), OtherMesh->Bounds.GetCenter());
					if (!ClosestMesh || Dist < Distance)
					{
						ClosestMesh = OtherMesh;
						Distance = Dist;
					}
				}

				Context->GetAsyncManager()->Start<FBridgeMeshesTask>(MeshIndex, Context->ConsolidatedEdges, Context->Meshes.IndexOfByKey(ClosestMesh));
			}
			else if (Context->BridgeMethod == EPCGExBridgeIslandMethod::MostEdges)
			{
				for (int i = 0; i < Context->Meshes.Num(); i++)
				{
					if (CurrentMesh == Context->Meshes[i]) { continue; }
					Context->GetAsyncManager()->Start<FBridgeMeshesTask>(MeshIndex, Context->ConsolidatedEdges, i);
				}
			}
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
	FPCGExBridgeEdgeIslandsContext* Context = Manager->GetContext<FPCGExBridgeEdgeIslandsContext>();


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
			const PCGExMesh::FVertex& OtherVtx = OtherMeshVertices[j];
			if (const double Dist = FVector::DistSquared(CurrentVtx.Position, OtherVtx.Position);
				Dist < Distance)
			{
				IndexA = CurrentVtx.PointIndex;
				IndexB = OtherVtx.PointIndex;
				Distance = Dist;
			}
		}
	}

	int32 EdgeIndex = -1;
	FPCGPoint& Bridge = Context->ConsolidatedEdges->NewPoint(EdgeIndex);
	Bridge.Transform.SetLocation(
		FMath::Lerp(
			Context->CurrentIO->GetInPoint(IndexA).Transform.GetLocation(),
			Context->CurrentIO->GetInPoint(IndexB).Transform.GetLocation(), 0.5));

	const PCGMetadataEntryKey BridgeKey = Bridge.MetadataEntry;
	UPCGMetadata* OutMetadataData = Context->ConsolidatedEdges->GetOut()->Metadata;
	OutMetadataData->FindOrCreateAttribute<int32>(PCGExGraph::EdgeStartAttributeName)->SetValue(BridgeKey, IndexA);
	OutMetadataData->FindOrCreateAttribute<int32>(PCGExGraph::EdgeEndAttributeName)->SetValue(BridgeKey, IndexB);

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
