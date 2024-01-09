// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExBridgeEdgeClusters.h"

#include "Data/PCGExPointIOMerger.h"
#include "Geometry/PCGExGeoDelaunay.h"

#define LOCTEXT_NAMESPACE "PCGExBridgeEdgeClusters"
#define PCGEX_NAMESPACE BridgeEdgeClusters

UPCGExBridgeEdgeClustersSettings::UPCGExBridgeEdgeClustersSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExData::EInit UPCGExBridgeEdgeClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

bool UPCGExBridgeEdgeClustersSettings::GetCacheAllClusteres() const { return true; }

PCGEX_INITIALIZE_ELEMENT(BridgeEdgeClusters)

FPCGExBridgeEdgeClustersContext::~FPCGExBridgeEdgeClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	ConsolidatedEdges = nullptr;

	VisitedClusters.Empty();
}


bool FPCGExBridgeEdgeClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BridgeEdgeClusters)

	PCGEX_FWD(BridgeMethod)

	return true;
}

bool FPCGExBridgeEdgeClustersElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBridgeEdgeClustersElement::Execute);

	PCGEX_CONTEXT(BridgeEdgeClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		Context->VisitedClusters.Empty();

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
			/* Batch-build all meshes since bCacheAllClusteres == true */
			if (Context->CurrentCluster->HasInvalidEdges())
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input edges are invalid. This will highly likely cause unexpected results."));
			}
		}
		Context->SetState(PCGExGraph::State_ProcessingEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		auto BridgeClusters = [&](const int32 ClusterIndex)
		{
			PCGExCluster::FCluster* CurrentCluster = Context->Clusters[ClusterIndex];

			const int32 ClusterNum = Context->Clusters.Num();
			EPCGExBridgeClusterMethod SafeMethod = Context->BridgeMethod;

			if (ClusterNum <= 4 && SafeMethod == EPCGExBridgeClusterMethod::Delaunay) { SafeMethod = EPCGExBridgeClusterMethod::MostEdges; }

			if (SafeMethod == EPCGExBridgeClusterMethod::Delaunay)
			{
				PCGExGeo::TDelaunayTriangulation3* Delaunay = new PCGExGeo::TDelaunayTriangulation3();
				TArray<FPCGPoint> Points;
				Points.SetNum(ClusterNum);
				for (int i = 0; i < Points.Num(); i++) { Points[i].Transform.SetLocation(Context->Clusters[i]->Bounds.GetCenter()); }
				if (Delaunay->PrepareFrom(Points))
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
									Context->GetAsyncManager()->Start<FBridgeClusteresTask>(A, Context->ConsolidatedEdges, B);
									UniqueEdges.Add(Hash);
								}
							}
						}
					}

					UniqueEdges.Empty();
				}

				PCGEX_DELETE(Delaunay)
			}
			else if (SafeMethod == EPCGExBridgeClusterMethod::LeastEdges)
			{
				Context->VisitedClusters.Add(CurrentCluster); // As to not connect to self or already connected

				PCGExCluster::FCluster* ClosestCluster = nullptr;
				double Distance = TNumericLimits<double>::Max();
				for (PCGExCluster::FCluster* OtherCluster : Context->Clusters)
				{
					if (Context->VisitedClusters.Contains(OtherCluster)) { continue; }
					const double Dist = FVector::DistSquared(CurrentCluster->Bounds.GetCenter(), OtherCluster->Bounds.GetCenter());
					if (!ClosestCluster || Dist < Distance)
					{
						ClosestCluster = OtherCluster;
						Distance = Dist;
					}
				}

				Context->GetAsyncManager()->Start<FBridgeClusteresTask>(ClusterIndex, Context->ConsolidatedEdges, Context->Clusters.IndexOfByKey(ClosestCluster));
			}
			else if (SafeMethod == EPCGExBridgeClusterMethod::MostEdges)
			{
				for (int i = 0; i < ClusterNum; i++)
				{
					if (CurrentCluster == Context->Clusters[i]) { continue; }
					Context->GetAsyncManager()->Start<FBridgeClusteresTask>(ClusterIndex, Context->ConsolidatedEdges, i);
				}
			}
		};

		if (Context->Process(BridgeClusters, Context->Clusters.Num() - 1, true))
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

bool FBridgeClusteresTask::ExecuteTask()
{
	FPCGExBridgeEdgeClustersContext* Context = Manager->GetContext<FPCGExBridgeEdgeClustersContext>();


	const TArray<PCGExCluster::FVertex>& CurrentClusterVertices = Context->Clusters[TaskIndex]->Vertices;
	const TArray<PCGExCluster::FVertex>& OtherClusterVertices = Context->Clusters[OtherClusterIndex]->Vertices;

	int32 IndexA = -1;
	int32 IndexB = -1;

	double Distance = TNumericLimits<double>::Max();

	//Brute force find closest points
	for (int i = 0; i < CurrentClusterVertices.Num(); i++)
	{
		const PCGExCluster::FVertex& CurrentVtx = CurrentClusterVertices[i];

		for (int j = 0; j < OtherClusterVertices.Num(); j++)
		{
			const PCGExCluster::FVertex& OtherVtx = OtherClusterVertices[j];
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
