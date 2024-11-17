// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
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

		TArray<FVector>* ProjectedPositions = nullptr;
		TMap<uint32, int32>* ProjectedHashMap = nullptr;

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
			InternalMesh->EditMesh(
				[&](FDynamicMesh3& InMesh)
				{
					InMesh.EnableVertexNormals(FVector3f(0, 0, 1));
				}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::MeshTopology, true);


			return true;
		}

		virtual void Output() override
		{
			if (!this->bIsProcessorValid) { return; }

			UE_LOG(LogTemp, Warning, TEXT("Output %llu | %d"), Settings->UID, EdgeDataFacade->Source->IOIndex)

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
			UDynamicMeshComponent* DynamicMeshComponent = NewObject<UDynamicMeshComponent>(TargetActor, MakeUniqueObjectName(TargetActor, UDynamicMeshComponent::StaticClass(), FName(ComponentName)), ObjectFlags);

			Settings->Topology.TemplateDescriptor.InitComponent(DynamicMeshComponent);

			DynamicMeshComponent->SetDynamicMesh(InternalMesh);
			if (UMaterialInterface* Material = Settings->Topology.Material.LoadSynchronous())
			{
				DynamicMeshComponent->SetMaterial(0, Material);
			}

			Context->ManagedObjects->Remove(InternalMesh);


			Context->AttachManageComponent(
				TargetActor, DynamicMeshComponent,
				FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false));

			Context->NotifyActors.Add(TargetActor);
		}

	protected:
		void FilterConstrainedEdgeScope(const int32 StartIndex, const int32 Count)
		{
			int32 LocalConstrainedEdgesNum = 0;

			if (EdgeFilterManager)
			{
				const int32 MaxIndex = StartIndex + Count;
				for (int i = StartIndex; i < MaxIndex; i++)
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
					InMesh.EnableVertexColors(
						FVector3f(
							Settings->Topology.DefaultVertexColor.Component(0),
							Settings->Topology.DefaultVertexColor.Component(1),
							Settings->Topology.DefaultVertexColor.Component(2)));
					
					const int32 VtxCount = InMesh.VertexCount();
					const TArray<FPCGPoint>& InPoints = VtxDataFacade->GetIn()->GetPoints();
					const TMap<uint32, int32>& HashMapRef = *ProjectedHashMap;

					for (int i = 0; i < VtxCount; i++)
					{
						const int32* WP = HashMapRef.Find(PCGEx::GH2(InMesh.GetVertex(i), CWTolerance));
						if (WP)
						{
							const FPCGPoint& Point = InPoints[*WP];
							InMesh.SetVertex(i, Point.Transform.GetLocation());
							InMesh.SetVertexColor(i, FVector3f(Point.Color[0], Point.Color[1], Point.Color[2]));
						}
					}
				}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::VertexColors | EDynamicMeshAttributeChangeFlags::VertexPositions, true);
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
		TArray<FVector> ProjectedPositions;
		TMap<uint32, int32> ProjectedHashMap;

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
				[WeakThis = TWeakPtr<TBatch>(SharedThis(this))]()
				{
					if (TSharedPtr<TBatch> This = WeakThis.Pin()) { This->OnProjectionComplete(); }
				};

			ProjectionTaskGroup->OnSubLoopStartCallback =
				[WeakThis = TWeakPtr<TBatch>(SharedThis(this))]
				(const int32 StartIndex, const int32 Count, const int32 LoopIdx)
				{
					FRandomStream Random(StartIndex + Count + LoopIdx);
					TSharedPtr<TBatch> This = WeakThis.Pin();
					if (!This) { return; }

					const int32 MaxIndex = StartIndex + Count;

					for (int i = StartIndex; i < MaxIndex; i++)
					{
						FVector V = This->ProjectionDetails.ProjectFlat(This->VtxDataFacade->Source->GetInPoint(i).Transform.GetLocation(), i);
						This->ProjectedPositions[i] = V + (Random.VRand() * 0.1); // Cheap triangulation edge case prevention
					}
				};

			ProjectionTaskGroup->StartSubLoops(VtxDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
		}

		virtual bool PrepareSingle(const TSharedPtr<T>& ClusterProcessor) override
		{
			ClusterProcessor->ProjectedPositions = &ProjectedPositions;
			ClusterProcessor->ProjectedHashMap = &ProjectedHashMap;
			PCGExClusterMT::TBatch<T>::PrepareSingle(ClusterProcessor);
			return true;
		}

	protected:
		void OnProjectionComplete()
		{
			const int32 NumVtx = VtxDataFacade->GetNum();
			ProjectedHashMap.Reserve(NumVtx);
			for (int i = 0; i < NumVtx; i++) { ProjectedHashMap.Add(PCGEx::GH2(FVector2D(ProjectedPositions[i]), CWTolerance), i); }

			PCGExClusterMT::TBatch<T>::Process();
		}
	};
}
