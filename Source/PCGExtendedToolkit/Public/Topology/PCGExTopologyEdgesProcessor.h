// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExTopology.h"
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

	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExCellConstraintsDetails CellConstraints;
	
	/** Projection settings. */
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
		bool bBuildExpandedNodes = false;
		bool bIsPreviewMode = false;

		TObjectPtr<UDynamicMesh> InternalMesh;

		TSharedPtr<PCGEx::FIndexLookup> VerticesLookup;
		TSharedPtr<TArray<PCGExCluster::FExpandedNode>> ExpandedNodes;

		TSharedPtr<PCGExTopology::FCellConstraints> CellConstraints;
		TArray<int8> ConstrainedEdgeFilterCache;

		TSharedPtr<PCGExClusterFilter::FManager> EdgeFilterManager;
		int32 ConstrainedEdgesNum = 0;

	public:
		TArray<FVector>* ProjectedPositions = nullptr;

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
				InClusterRef, this->VtxDataFacade->Source, this->EdgeDataFacade->Source, this->NodeIndexLookup,
				true, false, false);
		}

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override
		{
			this->EdgeDataFacade->bSupportsScopedGet = true;

			if (!PCGExClusterMT::TProcessor<TContext, TSettings>::Process(InAsyncManager)) { return false; }

#if PCGEX_ENGINE_VERSION > 503
			bIsPreviewMode = this->ExecutionContext->SourceComponent.Get()->IsInPreviewMode();
#endif

			ConstrainedEdgeFilterCache.Init(false, this->EdgeDataFacade->Source->GetNum());

			CellConstraints = MakeShared<PCGExTopology::FCellConstraints>(this->Settings->CellConstraints);
			InitConstraints();

			for (PCGExCluster::FNode& Node : *this->Cluster->Nodes) { Node.bValid = false; } // Invalidate all edges, triangulation will mark valid nodes to rebuild an index

			if (!this->Context->EdgeConstraintsFilterFactories.IsEmpty())
			{
				EdgeFilterManager = MakeShared<PCGExClusterFilter::FManager>(this->Cluster.ToSharedRef(), this->VtxDataFacade, this->EdgeDataFacade);
				EdgeFilterManager->bUseEdgeAsPrimary = true;
				if (!EdgeFilterManager->Init(this->Context, this->Context->EdgeConstraintsFilterFactories)) { return false; }
			}

			// IMPORTANT : Need to wait for projection to be completed.
			// Children should start work only in CompleteWork!!

			ExpandedNodes = this->Cluster->ExpandedNodes;

			if (!ExpandedNodes)
			{
				ExpandedNodes = this->Cluster->GetExpandedNodes(false);
				bBuildExpandedNodes = true;
				this->StartParallelLoopForRange(this->NumNodes);
			}

			InternalMesh = this->Context->ManagedObjects->template New<UDynamicMesh>();
			InternalMesh->EditMesh(
				[&](FDynamicMesh3& InMesh)
				{
					InMesh.EnableVertexNormals(FVector3f(0, 0, 1));
				}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::MeshTopology, true);


			return true;
		}

		virtual void ProcessSingleRangeIteration(int32 Iteration, const int32 LoopIdx, const int32 Count) override
		{
			*(ExpandedNodes->GetData() + Iteration) = PCGExCluster::FExpandedNode(this->Cluster, Iteration);
		}

		bool BuildValidNodeLookup()
		{
			VerticesLookup = MakeShared<PCGEx::FIndexLookup>(this->NumNodes);
			int32 ValidIndex = 0;

			InternalMesh->EditMesh(
				[&](FDynamicMesh3& InMesh)
				{
					for (int i = 0; i < this->NumNodes; i++)
					{
						if (!this->Cluster->GetNode(i)->bValid) { continue; }
						VerticesLookup->Set(i, InMesh.AppendVertex(this->Cluster->GetPos(i)));
						ValidIndex++;
					}
				}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::MeshTopology, true);


			if (ValidIndex == 0)
			{
				VerticesLookup.Reset();
				return false;
			}

			return true;
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
					ConstrainedEdgeFilterCache[i] = EdgeFilterManager->Test(*this->Cluster->GetEdge(i));
					if (ConstrainedEdgeFilterCache[i]) { LocalConstrainedEdgesNum++; }
				}
			}

			FPlatformAtomics::InterlockedAdd(&ConstrainedEdgesNum, LocalConstrainedEdgesNum);
		}
	};

	template <typename T>
	class TBatch : public PCGExClusterMT::TBatch<T>
	{
	protected:
		FPCGExGeo2DProjectionDetails ProjectionDetails;
		TArray<FVector> ProjectedPositions;

	public:
		TBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges):
			PCGExClusterMT::TBatch<T>(InContext, InVtx, InEdges)
		{
		}

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override
		{
			PCGExClusterMT::TBatch<T>::RegisterBuffersDependencies(FacadePreloader);
			FPCGExTopologyEdgesProcessorContext* Context = static_cast<FPCGExTopologyEdgesProcessorContext*>(this->ExecutionContext);
			const UPCGExTopologyEdgesProcessorSettings* Settings = Context->GetInputSettings<UPCGExTopologyEdgesProcessorSettings>();
			check(Settings);

			if (Settings->SupportsEdgeConstraints())
			{
				PCGExPointFilter::RegisterBuffersDependencies(this->ExecutionContext, Context->EdgeConstraintsFilterFactories, FacadePreloader);
			}
		}

		virtual void Process() override
		{
			FPCGExTopologyEdgesProcessorContext* Context = static_cast<FPCGExTopologyEdgesProcessorContext*>(this->ExecutionContext);
			const UPCGExTopologyEdgesProcessorSettings* Settings = Context->GetInputSettings<UPCGExTopologyEdgesProcessorSettings>();
			check(Settings);

			// Project positions
			ProjectionDetails = Settings->ProjectionDetails;
			if (!ProjectionDetails.Init(Context, this->VtxDataFacade)) { return; }

			PCGEx::InitArray(ProjectedPositions, this->VtxDataFacade->GetNum());

			PCGEX_ASYNC_GROUP_CHKD_VOID(this->AsyncManager, ProjectionTaskGroup)

			ProjectionTaskGroup->OnSubLoopStartCallback =
				[WeakThis = TWeakPtr<TBatch<T>>(this->SharedThis(this))]
				(const int32 StartIndex, const int32 Count, const int32 LoopIdx)
				{
					TSharedPtr<TBatch<T>> This = WeakThis.Pin();
					if (!This) { return; }

					const int32 MaxIndex = StartIndex + Count;

					for (int i = StartIndex; i < MaxIndex; i++)
					{
						This->ProjectedPositions[i] = This->ProjectionDetails.ProjectFlat(This->VtxDataFacade->Source->GetInPoint(i).Transform.GetLocation(), i);
					}
				};

			ProjectionTaskGroup->StartSubLoops(this->VtxDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());

			PCGExClusterMT::TBatch<T>::Process();
		}

		virtual bool PrepareSingle(const TSharedPtr<T>& ClusterProcessor) override
		{
			ClusterProcessor->ProjectedPositions = &ProjectedPositions;
			PCGExClusterMT::TBatch<T>::PrepareSingle(ClusterProcessor);
			return true;
		}
	};
}
