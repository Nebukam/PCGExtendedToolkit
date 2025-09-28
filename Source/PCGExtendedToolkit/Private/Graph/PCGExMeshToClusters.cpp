// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExMeshToClusters.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExPointIO.h"
#include "GameFramework/Actor.h"
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

TArray<FPCGPinProperties> UPCGExMeshToClustersSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGExGeo::DeclareGeoMeshImportInputs(ImportDetails, PinProperties);
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExMeshToClustersSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required)
	PCGEX_PIN_POINTS(FName("BaseMeshData"), "Vtx & edges that have been copied to point. Contains one graph per unique mesh asset.", Advanced)
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

	PCGEX_FWD(ImportDetails)
	if (!Context->ImportDetails.Validate(Context)) { return false; }
	Context->bWantsImport = Context->ImportDetails.WantsImport();

	if (Settings->StaticMeshInput == EPCGExInputValueType::Attribute)
	{
		PCGEX_VALIDATE_NAME_CONSUMABLE(Settings->StaticMeshAttribute)
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

	Context->BaseMeshDataCollection = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->BaseMeshDataCollection->OutputPin = FName("BaseMeshData");

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

			const UPCGBasePointData* TargetPoints = Context->CurrentIO->GetIn();
			const int32 NumTargets = TargetPoints->GetNumPoints();
			for (int i = 0; i < NumTargets; i++)
			{
				FSoftObjectPath Path = PathGetter->FetchSingle(PCGExData::FConstPoint(TargetPoints, i), FSoftObjectPath());

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
		Context->SetAsyncState(PCGExGraph::State_WritingClusters);

		const TSharedPtr<PCGExMT::FTaskManager> AsyncManager = Context->GetAsyncManager();

		const int32 NumTargets = Context->CurrentIO->GetIn()->GetNumPoints();
		for (int i = 0; i < NumTargets; i++)
		{
			const int32 MeshIdx = Context->MeshIdx[i];
			if (MeshIdx == -1) { continue; }
			PCGEX_LAUNCH(PCGExGraphTask::FCopyGraphToPoint, i, Context->CurrentIO, Context->GraphBuilders[MeshIdx], Context->VtxChildCollection, Context->EdgeChildCollection, &Context->TransformDetails)
		}
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExGraph::State_WritingClusters)
	{
		Context->BaseMeshDataCollection->StageOutputs();

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
		case EPCGExTriangulationType::Boundaries:
			Mesh->TriangulateMeshSynchronous();
			if (Mesh->HullIndices.IsEmpty() || Mesh->HullEdges.IsEmpty()) { return; }
			break;
		}

		if (!Mesh->bIsValid || Mesh->Vertices.IsEmpty()) { return; }

		const TSharedPtr<PCGExData::FPointIO> RootVtx = Context->RootVtx->Emplace_GetRef<UPCGExClusterNodesData>();

		if (!RootVtx) { return; }

		RootVtx->IOIndex = TaskIndex;

		UPCGBasePointData* VtxPoints = RootVtx->GetOut();
		PCGEX_MAKE_SHARED(RootVtxFacade, PCGExData::FFacade, RootVtx.ToSharedRef())

		bool bWantsColor = false;
		TArray<TSharedPtr<PCGExData::TBuffer<FVector2D>>> UVChannelsWriters;
		TArray<int32> UVChannels;
		TArray<FPCGAttributeIdentifier> UVIdentifiers;

		EPCGPointNativeProperties Allocations = EPCGPointNativeProperties::Transform;
		const FStaticMeshVertexBuffers* VertexBuffers = nullptr;

		const FPCGExGeoMeshImportDetails& ImportDetails = Context->ImportDetails;

		if (Context->bWantsImport)
		{
			VertexBuffers = &Mesh->LODResource->VertexBuffers;

			if (ImportDetails.bImportVertexColor && Mesh->bHasColorData)
			{
				Allocations |= EPCGPointNativeProperties::Color;
				bWantsColor = true;
			}

			if (const int32 NumTexCoords = Mesh->LODResource->GetNumTexCoords();
				!ImportDetails.UVChannelIndex.IsEmpty() && NumTexCoords >= 0)
			{
				UVChannels.Reserve(ImportDetails.UVChannelIndex.Num());
				UVChannelsWriters.Reserve(ImportDetails.UVChannelIndex.Num());
				UVIdentifiers.Reserve(ImportDetails.UVChannelIndex.Num());

				for (int i = 0; i < ImportDetails.UVChannelIndex.Num(); i++)
				{
					int32 Channel = ImportDetails.UVChannelIndex[i];
					const FPCGAttributeIdentifier& Id = ImportDetails.UVChannelId[i];

					if (Channel >= NumTexCoords)
					{
						if (ImportDetails.bCreatePlaceholders) { PCGExData::WriteMark(VtxPoints, Id, ImportDetails.Placeholder); }
						continue;
					}

					UVChannels.Add(Channel);
					UVIdentifiers.Add(Id);
				}
			}
		}

		auto InitUVWriters = [&]()
		{
			// UV channels attribute need to be initialized once we have the final number of points
			for (int32 i = 0; i < UVChannels.Num(); i++)
			{
				UVChannelsWriters.Add(RootVtxFacade->GetWritable(UVIdentifiers[i], FVector2D::ZeroVector, true, PCGExData::EBufferInit::New));
			}
		};

		const int32 NumUVChannels = Context->bWantsImport ? UVChannels.Num() : 0;


		if (Mesh->DesiredTriangulationType == EPCGExTriangulationType::Boundaries)
		{
			const int32 NumHullVertices = Mesh->HullIndices.Num();
			(void)PCGEx::SetNumPointsAllocated(VtxPoints, NumHullVertices, Allocations);
			InitUVWriters();

			TPCGValueRange<FTransform> OutTransforms = VtxPoints->GetTransformValueRange(false);

			int32 t = 0;
			TMap<int32, int32> IndicesRemap;
			IndicesRemap.Reserve(NumHullVertices);

#define PCGEX_BOUNDARY_PUSH IndicesRemap.Add(i, t); OutTransforms[t++].SetLocation(Mesh->Vertices[i]);

			if (bWantsColor)
			{
				const FColorVertexBuffer& ColorBuffer = VertexBuffers->ColorVertexBuffer;
				TPCGValueRange<FVector4> OutColors = VtxPoints->GetColorValueRange(false);

				if (!NumUVChannels)
				{
					// Color only
					for (int32 i : Mesh->HullIndices)
					{
						const int32 RawIndex = Mesh->RawIndices[i];
						OutColors[t] = FVector4(ColorBuffer.VertexColor(RawIndex));
						PCGEX_BOUNDARY_PUSH
					}
				}
				else
				{
					// Color + UVs
					for (int32 i : Mesh->HullIndices)
					{
						const int32 RawIndex = Mesh->RawIndices[i];
						OutColors[t] = FVector4(ColorBuffer.VertexColor(RawIndex));
						for (int u = 0; u < NumUVChannels; u++)
						{
							UVChannelsWriters[u]->SetValue(t, FVector2D(VertexBuffers->StaticMeshVertexBuffer.GetVertexUV(RawIndex, UVChannels[u])));
						}

						PCGEX_BOUNDARY_PUSH
					}
				}
			}
			else if (NumUVChannels)
			{
				// UVs only
				for (int32 i : Mesh->HullIndices)
				{
					const int32 RawIndex = Mesh->RawIndices[i];
					for (int u = 0; u < NumUVChannels; u++)
					{
						UVChannelsWriters[u]->SetValue(t, FVector2D(VertexBuffers->StaticMeshVertexBuffer.GetVertexUV(RawIndex, UVChannels[u])));
					}

					PCGEX_BOUNDARY_PUSH
				}
			}
			else
			{
				// No imports
				for (int32 i : Mesh->HullIndices) { PCGEX_BOUNDARY_PUSH }
			}

#undef PCGEX_BOUNDARY_PUSH

			Mesh->Edges.Empty();
			for (const uint64 Edge : Mesh->HullEdges)
			{
				uint32 A = 0;
				uint32 B = 0;
				PCGEx::H64(Edge, A, B);
				Mesh->Edges.Add(PCGEx::H64U(IndicesRemap[A], IndicesRemap[B]));
			}
		}
		else
		{
			(void)PCGEx::SetNumPointsAllocated(VtxPoints, Mesh->Vertices.Num(), Allocations);
			InitUVWriters();

			TPCGValueRange<FTransform> OutTransforms = VtxPoints->GetTransformValueRange(false);
			for (int i = 0; i < OutTransforms.Num(); i++) { OutTransforms[i].SetLocation(Mesh->Vertices[i]); }

			if (bWantsColor || NumUVChannels)
			{
				if (Mesh->DesiredTriangulationType == EPCGExTriangulationType::Dual)
				{
					// For dual graph we need to average triangle values for all imports
					// Mesh raw vertices has been mutated by `MakeDual` in order to facilitate that

					if (bWantsColor)
					{
						const FColorVertexBuffer& ColorBuffer = VertexBuffers->ColorVertexBuffer;
						TPCGValueRange<FVector4> OutColors = VtxPoints->GetColorValueRange(false);

						if (!NumUVChannels)
						{
							// Color only
							for (int i = 0; i < OutTransforms.Num(); i++)
							{
								const FIntVector3& Triangle = Mesh->Triangles[-(Mesh->RawIndices[i] + 1)];

								OutColors[i] = (FVector4(ColorBuffer.VertexColor(Triangle.X))
									+ FVector4(ColorBuffer.VertexColor(Triangle.Y))
									+ FVector4(ColorBuffer.VertexColor(Triangle.Z))) / 3;
							}
						}
						else
						{
							// Color + UVs
							for (int i = 0; i < OutTransforms.Num(); i++)
							{
								const FIntVector3& Triangle = Mesh->Triangles[-(Mesh->RawIndices[i] + 1)];

								OutColors[i] = (FVector4(ColorBuffer.VertexColor(Triangle.X))
									+ FVector4(ColorBuffer.VertexColor(Triangle.Y))
									+ FVector4(ColorBuffer.VertexColor(Triangle.Z))) / 3;

								for (int u = 0; u < NumUVChannels; u++)
								{
									FVector2D AverageUVs = FVector2D::ZeroVector;
									for (int t = 0; t < 3; t++) { AverageUVs += FVector2D(VertexBuffers->StaticMeshVertexBuffer.GetVertexUV(Triangle[t], UVChannels[u])); }
									AverageUVs /= 3;
									UVChannelsWriters[u]->SetValue(i, AverageUVs);
								}
							}
						}
					}
					else
					{
						// UVs only
						for (int i = 0; i < OutTransforms.Num(); i++)
						{
							const FIntVector3& Triangle = Mesh->Triangles[-(Mesh->RawIndices[i] + 1)];

							for (int u = 0; u < NumUVChannels; u++)
							{
								FVector2D AverageUVs = FVector2D::ZeroVector;
								for (int t = 0; t < 3; t++) { AverageUVs += FVector2D(VertexBuffers->StaticMeshVertexBuffer.GetVertexUV(Triangle[t], UVChannels[u])); }
								AverageUVs /= 3;
								UVChannelsWriters[u]->SetValue(i, AverageUVs);
							}
						}
					}
				}
				else
				{
					if (bWantsColor && NumUVChannels)
					{
						const FColorVertexBuffer& ColorBuffer = VertexBuffers->ColorVertexBuffer;
						TPCGValueRange<FVector4> OutColors = VtxPoints->GetColorValueRange(false);

						for (int i = 0; i < OutTransforms.Num(); i++)
						{
							const int32 RawIndex = Mesh->RawIndices[i];
							if (RawIndex >= 0)
							{
								OutColors[i] = FVector4(ColorBuffer.VertexColor(Mesh->RawIndices[i]));
								for (int u = 0; u < NumUVChannels; u++)
								{
									UVChannelsWriters[u]->SetValue(i, FVector2D(VertexBuffers->StaticMeshVertexBuffer.GetVertexUV(RawIndex, UVChannels[u])));
								}
							}
							else
							{
								const FIntVector3& Triangle = Mesh->Triangles[-(RawIndex + 1)];
								OutColors[i] = (FVector4(ColorBuffer.VertexColor(Mesh->RawIndices[Triangle.X]))
									+ FVector4(ColorBuffer.VertexColor(Mesh->RawIndices[Triangle.Y]))
									+ FVector4(ColorBuffer.VertexColor(Mesh->RawIndices[Triangle.Z]))) / 3;

								for (int u = 0; u < NumUVChannels; u++)
								{
									FVector2D AverageUVs = FVector2D::ZeroVector;

									for (int t = 0; t < 3; t++)
									{
										AverageUVs += FVector2D(VertexBuffers->StaticMeshVertexBuffer.GetVertexUV(Mesh->RawIndices[Triangle[t]], UVChannels[u]));
									}

									AverageUVs /= 3;
									UVChannelsWriters[u]->SetValue(i, AverageUVs);
								}
							}
						}
					}
					else if (bWantsColor)
					{
						// Color only
						const FColorVertexBuffer& ColorBuffer = VertexBuffers->ColorVertexBuffer;
						TPCGValueRange<FVector4> OutColors = VtxPoints->GetColorValueRange(false);

						for (int i = 0; i < OutTransforms.Num(); i++)
						{
							const int32 RawIndex = Mesh->RawIndices[i];
							if (RawIndex >= 0)
							{
								OutColors[i] = FVector4(ColorBuffer.VertexColor(Mesh->RawIndices[i]));
							}
							else
							{
								const FIntVector3& Triangle = Mesh->Triangles[-(RawIndex + 1)];
								OutColors[i] = (FVector4(ColorBuffer.VertexColor(Mesh->RawIndices[Triangle.X]))
									+ FVector4(ColorBuffer.VertexColor(Mesh->RawIndices[Triangle.Y]))
									+ FVector4(ColorBuffer.VertexColor(Mesh->RawIndices[Triangle.Z]))) / 3;
							}
						}
					}
					else
					{
						// UVs only
						for (int i = 0; i < OutTransforms.Num(); i++)
						{
							const int32 RawIndex = Mesh->RawIndices[i];
							if (RawIndex >= 0)
							{
								for (int u = 0; u < NumUVChannels; u++)
								{
									UVChannelsWriters[u]->SetValue(i, FVector2D(VertexBuffers->StaticMeshVertexBuffer.GetVertexUV(RawIndex, UVChannels[u])));
								}
							}
							else
							{
								const FIntVector3& Triangle = Mesh->Triangles[-(RawIndex + 1)];
								for (int u = 0; u < NumUVChannels; u++)
								{
									FVector2D AverageUVs = FVector2D::ZeroVector;

									for (int t = 0; t < 3; t++)
									{
										AverageUVs += FVector2D(VertexBuffers->StaticMeshVertexBuffer.GetVertexUV(Mesh->RawIndices[Triangle[t]], UVChannels[u]));
									}

									AverageUVs /= 3;
									UVChannelsWriters[u]->SetValue(i, AverageUVs);
								}
							}
						}
					}
				}
			}
		}

		TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(RootVtxFacade.ToSharedRef(), &Context->GraphBuilderDetails);
		GraphBuilder->Graph->InsertEdges(Mesh->Edges, -1);

		Context->GraphBuilders[TaskIndex] = GraphBuilder;

		// We need to write down UVs attributes before compiling the graph
		// as compilation will re-order points and metadata...
		// This is far from ideal but also much less of a headache.
		if (NumUVChannels > 0) { RootVtxFacade->WriteSynchronous(); }


		TWeakPtr<FPCGContextHandle> WeakHandle = Context->GetOrCreateHandle();
		GraphBuilder->OnCompilationEndCallback =
			[WeakHandle](const TSharedRef<PCGExGraph::FGraphBuilder>& InBuilder, const bool bSuccess)
			{
				if (!bSuccess) { return; }
				PCGEX_SHARED_TCONTEXT_VOID(MeshToClusters, WeakHandle)

				SharedContext.Get()->BaseMeshDataCollection->Add(InBuilder->NodeDataFacade->Source);
				SharedContext.Get()->BaseMeshDataCollection->Add(InBuilder->EdgesIO->Pairs);
			};

		GraphBuilder->CompileAsync(Context->GetAsyncManager(), true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
