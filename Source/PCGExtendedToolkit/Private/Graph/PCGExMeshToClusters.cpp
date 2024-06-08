// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExMeshToClusters.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Geometry/PCGExGeoMesh.h"
#include "Geometry/PCGExGeoVoronoi.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE MeshToClusters

namespace PCGExGeoTask
{
	class FLloydRelax3;
}

PCGExData::EInit UPCGExMeshToClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FPCGExMeshToClustersContext::~FPCGExMeshToClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(StaticMeshMap)
	PCGEX_DELETE_TARRAY(GraphBuilders)

	PCGEX_DELETE(RootVtx)
	PCGEX_DELETE(VtxChildCollection)
	PCGEX_DELETE(EdgeChildCollection)

	MeshIdx.Empty();
}

TArray<FPCGPinProperties> UPCGExMeshToClustersSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	return PinProperties;
}

FName UPCGExMeshToClustersSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

bool UPCGExMeshToClustersSettings::GetMainAcceptMultipleData() const { return false; }

PCGEX_INITIALIZE_ELEMENT(MeshToClusters)

FName UPCGExMeshToClustersSettings::GetMainInputLabel() const { return PCGEx::SourceTargetsLabel; }

bool FPCGExMeshToClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(MeshToClusters)

	if (Context->MainPoints->Pairs.Num() < 1)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing targets."));
		return false;
	}

	PCGEX_FWD(GraphBuilderSettings)
	PCGEX_FWD(TransformSettings)

	if (Settings->StaticMeshSource == EPCGExFetchType::Attribute)
	{
		PCGEX_VALIDATE_NAME(Settings->StaticMeshAttribute)
	}

	PCGExData::FPointIO* Targets = Context->MainPoints->Pairs[0];
	Context->MeshIdx.SetNum(Targets->GetNum());

	Context->StaticMeshMap = new PCGExGeo::FGeoStaticMeshMap();
	Context->StaticMeshMap->DesiredTriangulationType = Settings->GraphOutputType;

	Context->RootVtx = new PCGExData::FPointIOCollection(); // Make this pinless

	Context->VtxChildCollection = new PCGExData::FPointIOCollection();
	Context->VtxChildCollection->DefaultOutputLabel = Settings->GetMainOutputLabel();

	Context->EdgeChildCollection = new PCGExData::FPointIOCollection();
	Context->EdgeChildCollection->DefaultOutputLabel = PCGExGraph::OutputEdgesLabel;

	return true;
}

bool FPCGExMeshToClustersElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMeshToClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(MeshToClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (Settings->StaticMeshSource == EPCGExFetchType::Constant)
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

				PCGEx::FLocalToStringGetter* PathGetter = new PCGEx::FLocalToStringGetter();
				PathGetter->Capture(Selector);
				if (!PathGetter->SoftGrab(*Context->MainPoints->Pairs[0]))
				{
					PCGE_LOG(Error, GraphAndLog, FTEXT("Static mesh attribute does not exists on targets."));
					return false;
				}

				const TArray<FPCGPoint>& TargetPoints = Context->CurrentIO->GetIn()->GetPoints();
				for (int i = 0; i < TargetPoints.Num(); i++)
				{
					FSoftObjectPath Path = PathGetter->SoftGet(TargetPoints[i], TEXT(""));

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
						if (UObject* FoundObject = FindObject<AActor>(nullptr, *Path.ToString()))
						{
							if (AActor* SourceActor = Cast<AActor>(FoundObject))
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

			for (int i = 0; i < Context->StaticMeshMap->GSMs.Num(); i++)
			{
				PCGExGeo::FGeoStaticMesh* GSM = Context->StaticMeshMap->GSMs[i];
				Context->GetAsyncManager()->Start<PCGExMeshToCluster::FExtractMeshAndBuildGraph>(i, nullptr, GSM);
			}

			// Preload all & build local graphs to copy to points later on			
			Context->SetAsyncState(PCGExGeo::State_ExtractingMesh);
		}
	}

	if (Context->IsState(PCGExGeo::State_ExtractingMesh))
	{
		PCGEX_WAIT_ASYNC

		Context->SetAsyncState(PCGExMT::State_ProcessingPoints);
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto ProcessTarget = [&](const int32 TargetIndex, const PCGExData::FPointIO& PointIO)
		{
			const int32 MeshIdx = Context->MeshIdx[TargetIndex];

			if (MeshIdx == -1) { return; }

			Context->GetAsyncManager()->Start<PCGExGraphTask::FCopyGraphToPoint>(
				TargetIndex, Context->CurrentIO, Context->GraphBuilders[MeshIdx],
				Context->VtxChildCollection, Context->EdgeChildCollection, &Context->TransformSettings);
		};

		if (!Context->ProcessCurrentPoints(ProcessTarget)) { return false; }

		Context->SetAsyncState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		PCGEX_WAIT_ASYNC

		Context->VtxChildCollection->OutputTo(Context);
		Context->EdgeChildCollection->OutputTo(Context);

		Context->Done();
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

namespace PCGExMeshToCluster
{
	bool PCGExMeshToCluster::FExtractMeshAndBuildGraph::ExecuteTask()
	{
		FPCGExMeshToClustersContext* Context = static_cast<FPCGExMeshToClustersContext*>(Manager->Context);
		PCGEX_SETTINGS(MeshToClusters)

		switch (Mesh->DesiredTriangulationType) {
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
		

		PCGExData::FPointIO& RootVtx = Context->RootVtx->Emplace_GetRef();
		RootVtx.IOIndex = TaskIndex;
		RootVtx.SetNumInitialized(Mesh->Vertices.Num());
		TArray<FPCGPoint>& VtxPoints = RootVtx.GetOut()->GetMutablePoints();

		PCGExGraph::FGraphBuilder* GraphBuilder = new PCGExGraph::FGraphBuilder(RootVtx, &Context->GraphBuilderSettings);
		Context->GraphBuilders[TaskIndex] = GraphBuilder;

		for (int i = 0; i < VtxPoints.Num(); i++)
		{
			FPCGPoint& NewVtx = VtxPoints[i];
			NewVtx.Transform.SetLocation(Mesh->Vertices[i]);
		}

		GraphBuilder->Graph->InsertEdges(Mesh->Edges, -1);
		GraphBuilder->Compile(Context);

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
