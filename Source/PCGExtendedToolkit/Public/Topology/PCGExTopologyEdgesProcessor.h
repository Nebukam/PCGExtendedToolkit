// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDynamicMeshComponent.h"
#include "PCGExTopology.h"
#include "Geometry/PCGExGeoMesh.h"
#include "Graph/PCGExClusterMT.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Graph/Filters/PCGExClusterFilter.h"

#include "PCGExTopologyEdgesProcessor.generated.h"

UCLASS(Abstract, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTopologyEdgesProcessorSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(TopologyProcessor, "Topology", "Base processor to output meshes from clusters");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorPrimitives; }
#endif

	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

	virtual bool SupportsEdgeConstraints() const { return true; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

public:
	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionDetails ProjectionDetails;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExCellConstraintsDetails Constraints;

	/** Topology settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExTopologyDetails Topology;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TSoftObjectPtr<AActor> TargetActor;

	/** Comma separated tags */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FString CommaSeparatedComponentTags = TEXT("PCGExTopology");

	/** Specify a list of functions to be called on the target actor after dynamic mesh creation. Functions need to be parameter-less and with "CallInEditor" flag enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FName> PostProcessFunctionNames;

private:
	friend class FPCGExTopologyEdgesProcessorElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTopologyEdgesProcessorContext : FPCGExEdgesProcessorContext
{
	friend class FPCGExTopologyEdgesProcessorElement;
	TArray<TObjectPtr<const UPCGExFilterFactoryBase>> EdgeConstraintsFilterFactories;

	TSharedPtr<PCGExTopology::FHoles> Holes;

	TArray<FString> ComponentTags;
	TSet<AActor*> NotifyActors;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTopologyEdgesProcessorElement : public FPCGExEdgesProcessorElement
{
protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
};

namespace PCGExTopologyEdges
{
	const FName SourceEdgeConstrainsFiltersLabel = FName("ConstrainedEdgeFilters");

	template <typename TContext, typename TSettings>
	class TProcessor : public PCGExClusterMT::TProcessor<TContext, TSettings>
	{
	protected:
		using PCGExClusterMT::TProcessor<TContext, TSettings>::ExecutionContext;
		using PCGExClusterMT::TProcessor<TContext, TSettings>::Settings;
		using PCGExClusterMT::TProcessor<TContext, TSettings>::Context;

		const FVector2D CWTolerance = FVector2D(1 / 0.001);
		bool bIsPreviewMode = false;

		TSharedPtr<PCGExTopology::FCell> WrapperCell;
		TObjectPtr<UDynamicMesh> InternalMesh;

		TSharedPtr<PCGEx::FIndexLookup> VerticesLookup;

		TSharedPtr<PCGExTopology::FCellConstraints> CellsConstraints;
		TArray<int8> ConstrainedEdgeFilterCache;

		TSharedPtr<PCGExClusterFilter::FManager> EdgeFilterManager;
		int32 ConstrainedEdgesNum = 0;

	public:
		using PCGExClusterMT::TProcessor<TContext, TSettings>::NodeIndexLookup;
		using PCGExClusterMT::TProcessor<TContext, TSettings>::Cluster;
		using PCGExClusterMT::TProcessor<TContext, TSettings>::VtxDataFacade;
		using PCGExClusterMT::TProcessor<TContext, TSettings>::EdgeDataFacade;

		TSharedPtr<TArray<FVector>> ProjectedPositions;
		TSharedPtr<TMap<uint32, int32>> ProjectedHashMap;

		TObjectPtr<UDynamicMesh> GetInternalMesh() { return InternalMesh; }

		TProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: PCGExClusterMT::TProcessor<TContext, TSettings>(InVtxDataFacade, InEdgeDataFacade)
		{
			static_assert(std::is_base_of_v<FPCGExTopologyEdgesProcessorContext, TContext>, "TContext must inherit from FPCGExTopologyProcessorContext");
			static_assert(std::is_base_of_v<UPCGExTopologyEdgesProcessorSettings, TSettings>, "TSettings must inherit from UPCGExTopologyProcessorSettings");
		}

		virtual ~TProcessor() override
		{
		}

		virtual void InitConstraints()
		{
		}

		virtual TSharedPtr<PCGExCluster::FCluster> HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef) override
		{
			// Create a light working copy with nodes only, will be deleted.
			return MakeShared<PCGExCluster::FCluster>(
				InClusterRef, VtxDataFacade->Source, EdgeDataFacade->Source, NodeIndexLookup,
				true, false, false);
		}

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override
		{
			EdgeDataFacade->bSupportsScopedGet = true;

			if (!PCGExClusterMT::TProcessor<TContext, TSettings>::Process(InAsyncManager)) { return false; }

#if PCGEX_ENGINE_VERSION > 503
			bIsPreviewMode = ExecutionContext->SourceComponent.Get()->IsInPreviewMode();
#endif

			ConstrainedEdgeFilterCache.Init(false, EdgeDataFacade->Source->GetNum());

			CellsConstraints = MakeShared<PCGExTopology::FCellConstraints>(Settings->Constraints);
			if (Settings->Constraints.bOmitWrappingBounds) { CellsConstraints->BuildWrapperCell(Cluster.ToSharedRef(), *ProjectedPositions); }
			CellsConstraints->Holes = Context->Holes;

			InitConstraints();

			for (PCGExCluster::FNode& Node : *Cluster->Nodes) { Node.bValid = false; } // Invalidate all edges, triangulation will mark valid nodes to rebuild an index

			if (!Context->EdgeConstraintsFilterFactories.IsEmpty())
			{
				EdgeFilterManager = MakeShared<PCGExClusterFilter::FManager>(Cluster.ToSharedRef(), VtxDataFacade, EdgeDataFacade);
				EdgeFilterManager->bUseEdgeAsPrimary = true;
				if (!EdgeFilterManager->Init(Context, Context->EdgeConstraintsFilterFactories)) { return false; }
			}

			// IMPORTANT : Need to wait for projection to be completed.
			// Children should start work only in CompleteWork!!

			InternalMesh = Context->ManagedObjects->template New<UDynamicMesh>();
			InternalMesh->InitializeMesh();
			return true;
		}

		virtual void Output() override
		{
			if (!this->bIsProcessorValid) { return; }
			if (Settings->Topology.bCombinesAllTopologies) { return; }

			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExPathSplineMesh::FProcessor::Output);

			AActor* TargetActor = Settings->TargetActor.Get() ? Settings->TargetActor.Get() : ExecutionContext->GetTargetActor(nullptr);

			if (!TargetActor)
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Invalid target actor."));
				return;
			}

			const FString ComponentName = TEXT("PCGDynamicMeshComponent");
			const EObjectFlags ObjectFlags = (bIsPreviewMode ? RF_Transient : RF_NoFlags);
			UPCGExDynamicMeshComponent* DynamicMeshComponent = NewObject<UPCGExDynamicMeshComponent>(TargetActor, MakeUniqueObjectName(TargetActor, UPCGExDynamicMeshComponent::StaticClass(), FName(ComponentName)), ObjectFlags);

			Settings->Topology.TemplateDescriptor.InitComponent(DynamicMeshComponent);

			DynamicMeshComponent->SetDynamicMesh(InternalMesh);
			if (UMaterialInterface* Material = Settings->Topology.Material.LoadSynchronous())
			{
				DynamicMeshComponent->SetMaterial(0, Material);
			}

			DynamicMeshComponent->ComponentTags.Reserve(DynamicMeshComponent->ComponentTags.Num() + Context->ComponentTags.Num());
			for (const FString& ComponentTag : Context->ComponentTags) { DynamicMeshComponent->ComponentTags.Add(FName(ComponentTag)); }

			Context->ManagedObjects->Remove(InternalMesh);

			Context->AttachManagedComponent(
				TargetActor, DynamicMeshComponent,
				FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false));

			Context->NotifyActors.Add(TargetActor);
		}

		virtual void Cleanup() override
		{
			PCGExClusterMT::TProcessor<TContext, TSettings>::Cleanup();
			CellsConstraints->Cleanup();
		}

	protected:
		void FilterConstrainedEdgeScope(const PCGExMT::FScope& Scope)
		{
			int32 LocalConstrainedEdgesNum = 0;

			if (EdgeFilterManager)
			{
				for (int i = Scope.Start; i < Scope.End; i++)
				{
					ConstrainedEdgeFilterCache[i] = EdgeFilterManager->Test(*Cluster->GetEdge(i));
					if (ConstrainedEdgeFilterCache[i]) { LocalConstrainedEdgesNum++; }
				}
			}

			FPlatformAtomics::InterlockedAdd(&ConstrainedEdgesNum, LocalConstrainedEdgesNum);
		}

		void ApplyPointData()
		{
			InternalMesh->EditMesh(
				[&](FDynamicMesh3& InMesh)
				{
					const int32 VtxCount = InMesh.MaxVertexID();
					const TArray<FPCGPoint>& InPoints = VtxDataFacade->GetIn()->GetPoints();
					const TMap<uint32, int32>& HashMapRef = *ProjectedHashMap;


					FVector4f DefaultVertexColor = FVector4f(Settings->Topology.DefaultVertexColor);

					InMesh.EnableAttributes();
					InMesh.Attributes()->EnablePrimaryColors();
					UE::Geometry::FDynamicMeshColorOverlay* Colors = InMesh.Attributes()->PrimaryColors();

					TArray<int32> ElemIDs;
					ElemIDs.SetNum(VtxCount);

					for (int i = 0; i < VtxCount; i++)
					{
						const int32* WP = HashMapRef.Find(PCGEx::GH2(InMesh.GetVertex(i), CWTolerance));
						if (WP)
						{
							const FPCGPoint& Point = InPoints[*WP];
							InMesh.SetVertex(i, Point.Transform.GetLocation());
							ElemIDs[i] = Colors->AppendElement(FVector4f(Point.Color));
						}
						else
						{
							ElemIDs[i] = Colors->AppendElement(DefaultVertexColor);
						}
					}

					for (int32 TriangleID : InMesh.TriangleIndicesItr())
					{
						UE::Geometry::FIndex3i Triangle = InMesh.GetTriangle(TriangleID);
						Colors->SetTriangle(TriangleID, UE::Geometry::FIndex3i(ElemIDs[Triangle.A], ElemIDs[Triangle.B], ElemIDs[Triangle.C]));
					}
				}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, true);
		}
	};

	template <typename T>
	class TBatch : public PCGExClusterMT::TBatch<T>
	{
	protected:
		const FVector2D CWTolerance = FVector2D(1 / 0.001);

		using PCGExClusterMT::TBatch<T>::SharedThis;
		using PCGExClusterMT::TBatch<T>::AsyncManager;

		using PCGExClusterMT::TBatch<T>::ExecutionContext;
		using PCGExClusterMT::TBatch<T>::NodeIndexLookup;

		FPCGExGeo2DProjectionDetails ProjectionDetails;
		TSharedPtr<TArray<FVector>> ProjectedPositions;
		TSharedPtr<TMap<uint32, int32>> ProjectedHashMap;

	public:
		using PCGExClusterMT::TBatch<T>::VtxDataFacade;

		TBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges):
			PCGExClusterMT::TBatch<T>(InContext, InVtx, InEdges)
		{
		}

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override
		{
			PCGExClusterMT::TBatch<T>::RegisterBuffersDependencies(FacadePreloader);

			PCGEX_TYPED_CONTEXT_AND_SETTINGS(TopologyEdgesProcessor)

			check(Settings);

			if (Settings->SupportsEdgeConstraints())
			{
				PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->EdgeConstraintsFilterFactories, FacadePreloader);
			}
		}

		virtual void Process() override
		{
			PCGEX_TYPED_CONTEXT_AND_SETTINGS(TopologyEdgesProcessor)

			// Project positions
			ProjectionDetails = Settings->ProjectionDetails;
			if (!ProjectionDetails.Init(Context, VtxDataFacade)) { return; }

			PCGEx::InitArray(ProjectedPositions, VtxDataFacade->GetNum());

			PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ProjectionTaskGroup)

			ProjectionTaskGroup->OnCompleteCallback =
				[PCGEX_ASYNC_THIS_CAPTURE]()
				{
					PCGEX_ASYNC_THIS
					This->OnProjectionComplete();
				};

			ProjectionTaskGroup->OnSubLoopStartCallback =
				[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
				{
					PCGEX_ASYNC_THIS
					TArray<FVector>& PP = *This->ProjectedPositions;
					This->ProjectionDetails.ProjectFlat(This->VtxDataFacade, PP, Scope);
				};

			ProjectionTaskGroup->StartSubLoops(VtxDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
		}

		virtual bool PrepareSingle(const TSharedPtr<T>& ClusterProcessor) override
		{
			ClusterProcessor->ProjectedPositions = ProjectedPositions;
			ClusterProcessor->ProjectedHashMap = ProjectedHashMap;
			PCGExClusterMT::TBatch<T>::PrepareSingle(ClusterProcessor);
			return true;
		}

		virtual void Output() override
		{
			if (!this->bIsBatchValid) { return; }

			PCGEX_TYPED_CONTEXT_AND_SETTINGS(TopologyEdgesProcessor)

			if (Settings->Topology.bCombinesAllTopologies)
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Merged topology is not implemented yet."));
				/*
								TObjectPtr<UDynamicMesh> InternalMesh = Context->ManagedObjects->template New<UDynamicMesh>();
								InternalMesh->InitializeMesh();
								InternalMesh->EditMesh(
									[&](FDynamicMesh3& InMesh)
									{
										
										InMesh.EnableVertexNormals(FVector3f(0, 0, 1));
									}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::MeshTopology, true);
								
								bool bIsPreviewMode = false;
								
				#if PCGEX_ENGINE_VERSION > 503
								bIsPreviewMode = ExecutionContext->SourceComponent.Get()->IsInPreviewMode();
				#endif
								
								TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExPathSplineMesh::FProcessor::Output);
				
								// TODO : Resolve per-point target actor...? irk.
								AActor* TargetActor = Settings->TargetActor.Get() ? Settings->TargetActor.Get() : ExecutionContext->GetTargetActor(nullptr);
				
								if (!TargetActor)
								{
									PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Invalid target actor."));
									return;
								}
				
								const FString ComponentName = TEXT("PCGDynamicMeshComponent");
								const EObjectFlags ObjectFlags = (bIsPreviewMode ? RF_Transient : RF_NoFlags);
								UPCGExDynamicMeshComponent* DynamicMeshComponent = NewObject<UPCGExDynamicMeshComponent>(TargetActor, MakeUniqueObjectName(TargetActor, UPCGExDynamicMeshComponent::StaticClass(), FName(ComponentName)), ObjectFlags);
				
								Settings->Topology.TemplateDescriptor.InitComponent(DynamicMeshComponent);
				
								DynamicMeshComponent->SetDynamicMesh(InternalMesh);
								if (UMaterialInterface* Material = Settings->Topology.Material.LoadSynchronous())
								{
									DynamicMeshComponent->SetMaterial(0, Material);
								}
				
								Context->ManagedObjects->Remove(InternalMesh);
				
								Context->AttachManagedComponent(
									TargetActor, DynamicMeshComponent,
									FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false));
				
								Context->NotifyActors.Add(TargetActor);
								*/
			}
			else
			{
				PCGExClusterMT::TBatch<T>::Output();
			}
		}

	protected:
		void OnProjectionComplete()
		{
			const int32 NumVtx = VtxDataFacade->GetNum();
			ProjectedHashMap = MakeShared<TMap<uint32, int32>>();
			ProjectedHashMap->Reserve(NumVtx);

			TMap<uint32, int32>& MP = *ProjectedHashMap;
			const TArray<FVector>& PP = *ProjectedPositions;
			for (int i = 0; i < NumVtx; i++) { MP.Add(PCGEx::GH2(FVector2D(PP[i]), CWTolerance), i); }

			PCGExClusterMT::TBatch<T>::Process();
		}
	};
}
