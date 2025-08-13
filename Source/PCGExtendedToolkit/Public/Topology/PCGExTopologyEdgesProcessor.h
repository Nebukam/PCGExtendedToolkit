// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExContext.h"
#include "PCGComponent.h"
#include "PCGExDynamicMeshComponent.h"
#include "PCGExTopology.h"
#include "Data/PCGDynamicMeshData.h"
#include "Geometry/PCGExGeoMesh.h"
#include "Graph/PCGExClusterMT.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Transform/PCGExTransform.h"

#include "PCGExTopologyEdgesProcessor.generated.h"

UENUM()
enum class EPCGExTopologyOutputMode : uint8
{
	Legacy         = 0 UMETA(DisplayName = "Legacy (Spawn Mesh)", ToolTip="Spawns a dynamic mesh (Legacy)."),
	PCGDynamicMesh = 1 UMETA(DisplayName = "PCG Dynamic Mesh", ToolTip="Creates a PCG dynamic mesh."),
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class PCGEXTENDEDTOOLKIT_API UPCGExTopologyEdgesProcessorSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(TopologyProcessor, "Topology", "Base processor to output meshes from clusters");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->NodeColorTopology); }
	virtual EPCGSettingsType GetType() const override { return OutputMode == EPCGExTopologyOutputMode::Legacy ? EPCGSettingsType::Spawner : EPCGSettingsType::DynamicMesh; }
#endif

	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

	virtual bool SupportsEdgeConstraints() const { return true; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

#if WITH_EDITOR
	virtual void ApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins) override;
#endif

	//~End UPCGSettings

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExTopologyOutputMode OutputMode = EPCGExTopologyOutputMode::PCGDynamicMesh;

	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionDetails ProjectionDetails;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExCellConstraintsDetails Constraints;

	/** Topology settings. Some settings will be ignored based on selected output mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExTopologyDetails Topology;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="OutputMode == EPCGExTopologyOutputMode::Legacy", EditConditionHides))
	TSoftObjectPtr<AActor> TargetActor;

	/** Comma separated tags */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="OutputMode == EPCGExTopologyOutputMode::Legacy", EditConditionHides))
	FString CommaSeparatedComponentTags = TEXT("PCGExTopology");

	/** Specify a list of functions to be called on the target actor after dynamic mesh creation. Functions need to be parameter-less and with "CallInEditor" flag enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="OutputMode == EPCGExTopologyOutputMode::Legacy", EditConditionHides))
	TArray<FName> PostProcessFunctionNames;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="OutputMode == EPCGExTopologyOutputMode::Legacy", EditConditionHides))
	FPCGExAttachmentRules AttachmentRules;

protected:
	virtual bool IsCacheable() const override { return false; }

private:
	friend class FPCGExTopologyEdgesProcessorElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExTopologyEdgesProcessorContext : FPCGExEdgesProcessorContext
{
	friend class FPCGExTopologyEdgesProcessorElement;
	TArray<TObjectPtr<const UPCGExFilterFactoryData>> EdgeConstraintsFilterFactories;

	TSharedPtr<PCGExTopology::FHoles> Holes;
	TSharedPtr<PCGExData::FFacade> HolesFacade;
	TArray<TSharedPtr<TMap<uint32, int32>>> HashMaps;

	TArray<FString> ComponentTags;

	virtual void RegisterAssetDependencies() override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExTopologyEdgesProcessorElement : public FPCGExEdgesProcessorElement
{
protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
};

namespace PCGExTopologyEdges
{
	const FName SourceEdgeConstrainsFiltersLabel = FName("ConstrainedEdgeFilters");

	template <typename TContext, typename TSettings>
	class PCGEXTENDEDTOOLKIT_API TProcessor : public PCGExClusterMT::TProcessor<TContext, TSettings>
	{
	protected:
		using PCGExClusterMT::TProcessor<TContext, TSettings>::ExecutionContext;
		using PCGExClusterMT::TProcessor<TContext, TSettings>::Settings;
		using PCGExClusterMT::TProcessor<TContext, TSettings>::Context;

		TSharedPtr<PCGExTopology::FHoles> Holes;

		const FVector2D CWTolerance = FVector2D(1 / 0.001);
		bool bIsPreviewMode = false;

		TSharedPtr<PCGExTopology::FCell> WrapperCell;
		TObjectPtr<UDynamicMesh> InternalMesh;
		UPCGDynamicMeshData* InternalMeshData = nullptr;

		TSharedPtr<PCGEx::FIndexLookup> VerticesLookup;

		TSharedPtr<PCGExTopology::FCellConstraints> CellsConstraints;

		int32 ConstrainedEdgesNum = 0;

	public:
		using PCGExClusterMT::TProcessor<TContext, TSettings>::NodeIndexLookup;
		using PCGExClusterMT::TProcessor<TContext, TSettings>::Cluster;
		using PCGExClusterMT::TProcessor<TContext, TSettings>::VtxDataFacade;
		using PCGExClusterMT::TProcessor<TContext, TSettings>::EdgeDataFacade;
		using PCGExClusterMT::TProcessor<TContext, TSettings>::EdgeFilterFactories;
		using PCGExClusterMT::TProcessor<TContext, TSettings>::EdgeFilterCache;
		using PCGExClusterMT::TProcessor<TContext, TSettings>::DefaultEdgeFilterValue;

		TSharedPtr<TMap<uint32, int32>> ProjectedHashMap;

		TObjectPtr<UDynamicMesh> GetInternalMesh() { return InternalMesh; }

		TProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: PCGExClusterMT::TProcessor<TContext, TSettings>(InVtxDataFacade, InEdgeDataFacade)
		{
			static_assert(std::is_base_of_v<FPCGExTopologyEdgesProcessorContext, TContext>, "TContext must inherit from FPCGExTopologyProcessorContext");
			static_assert(std::is_base_of_v<UPCGExTopologyEdgesProcessorSettings, TSettings>, "TSettings must inherit from UPCGExTopologyProcessorSettings");
			DefaultEdgeFilterValue = false;
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

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override
		{
			EdgeDataFacade->bSupportsScopedGet = true;
			EdgeFilterFactories = &Context->EdgeConstraintsFilterFactories;

			ProjectedHashMap = Context->HashMaps[VtxDataFacade->Source->IOIndex]; 
			
			if (!PCGExClusterMT::TProcessor<TContext, TSettings>::Process(InAsyncManager)) { return false; }

			if (Context->HolesFacade) { Holes = Context->Holes ? Context->Holes : MakeShared<PCGExTopology::FHoles>(Context, Context->HolesFacade.ToSharedRef(), this->ProjectionDetails); }

			bIsPreviewMode = ExecutionContext->GetComponent()->IsInPreviewMode();

			CellsConstraints = MakeShared<PCGExTopology::FCellConstraints>(Settings->Constraints);
			if (Settings->Constraints.bOmitWrappingBounds) { CellsConstraints->BuildWrapperCell(Cluster.ToSharedRef(), *this->ProjectedVtxPositions.Get()); }
			CellsConstraints->Holes = Holes;

			InitConstraints();

			for (PCGExCluster::FNode& Node : *Cluster->Nodes) { Node.bValid = false; } // Invalidate all edges, triangulation will mark valid nodes to rebuild an index

			// IMPORTANT : Need to wait for projection to be completed.
			// Children should start work only in CompleteWork!!

			if (Settings->OutputMode == EPCGExTopologyOutputMode::PCGDynamicMesh)
			{
				InternalMeshData = Context->ManagedObjects->template New<UPCGDynamicMeshData>();
				if (!InternalMeshData) { return false; }
			}

			InternalMesh = Context->ManagedObjects->template New<UDynamicMesh>();
			InternalMesh->InitializeMesh();

			if (InternalMeshData)
			{
				InternalMeshData->Initialize(InternalMesh, true);
				InternalMesh = InternalMeshData->GetMutableDynamicMesh();
				if (UMaterialInterface* Material = Settings->Topology.Material.Get()) { InternalMeshData->SetMaterials({Material}); }
			}

			return true;
		}

		virtual void Output() override
		{
			if (!this->bIsProcessorValid) { return; }

			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExPathSplineMesh::FProcessor::Output);

			if (InternalMeshData)
			{
				TSet<FString> MeshTags;

				EdgeDataFacade->Source->Tags->DumpTo(MeshTags);
				VtxDataFacade->Source->Tags->DumpTo(MeshTags);

				Context->StageOutput(InternalMeshData, PCGExTopology::MeshOutputLabel, MeshTags, true, false, false);

				return;
			}

			AActor* TargetActor = Settings->TargetActor.Get() ? Settings->TargetActor.Get() : ExecutionContext->GetTargetActor(nullptr);

			if (!TargetActor)
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Invalid target actor."));
				return;
			}

			const FString ComponentName = TEXT("PCGDynamicMeshComponent");
			const EObjectFlags ObjectFlags = (bIsPreviewMode ? RF_Transient : RF_NoFlags);
			UPCGExDynamicMeshComponent* DynamicMeshComponent = NewObject<UPCGExDynamicMeshComponent>(TargetActor, MakeUniqueObjectName(TargetActor, UPCGExDynamicMeshComponent::StaticClass(), FName(ComponentName)), ObjectFlags);

			// Needed otherwise triggers updates in a loop
			Context->GetMutableComponent()->IgnoreChangeOriginDuringGenerationWithScope(
				DynamicMeshComponent, [&]()
				{
					Settings->Topology.TemplateDescriptor.InitComponent(DynamicMeshComponent);
					DynamicMeshComponent->SetDynamicMesh(InternalMesh);
					if (UMaterialInterface* Material = Settings->Topology.Material.Get())
					{
						DynamicMeshComponent->SetMaterial(0, Material);
					}
				});

			DynamicMeshComponent->ComponentTags.Reserve(DynamicMeshComponent->ComponentTags.Num() + Context->ComponentTags.Num());
			for (const FString& ComponentTag : Context->ComponentTags) { DynamicMeshComponent->ComponentTags.Add(FName(ComponentTag)); }

			Context->ManagedObjects->Remove(InternalMesh);
			Context->AttachManagedComponent(TargetActor, DynamicMeshComponent, Settings->AttachmentRules.GetRules());
			Context->AddNotifyActor(TargetActor);
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
			PCGEX_SCOPE_LOOP(i) { if (EdgeFilterCache[i]) { LocalConstrainedEdgesNum++; } }
			FPlatformAtomics::InterlockedAdd(&ConstrainedEdgesNum, LocalConstrainedEdgesNum);
		}

		void ApplyPointData()
		{
			FTransform Transform = Settings->OutputMode == EPCGExTopologyOutputMode::PCGDynamicMesh ? Context->GetComponent()->GetOwner()->GetTransform() : FTransform::Identity;
			Transform.SetScale3D(FVector::OneVector);
			Transform.SetRotation(FQuat::Identity);

			InternalMesh->EditMesh(
				[&](FDynamicMesh3& InMesh)
				{
					const int32 VtxCount = InMesh.MaxVertexID();
					const TConstPCGValueRange<FTransform> InTransforms = VtxDataFacade->GetIn()->GetConstTransformValueRange();
					const TConstPCGValueRange<FVector4> InColors = VtxDataFacade->GetIn()->GetConstColorValueRange();
					const TMap<uint32, int32>& HashMapRef = *ProjectedHashMap;

					FVector4f DefaultVertexColor = FVector4f(Settings->Topology.DefaultVertexColor);

					InMesh.EnableAttributes();
					InMesh.Attributes()->EnablePrimaryColors();
					InMesh.Attributes()->EnableMaterialID();

					UE::Geometry::FDynamicMeshColorOverlay* Colors = InMesh.Attributes()->PrimaryColors();
					UE::Geometry::FDynamicMeshMaterialAttribute* MaterialID = InMesh.Attributes()->GetMaterialID();

					TArray<int32> ElemIDs;
					ElemIDs.SetNum(VtxCount);

					for (int i = 0; i < VtxCount; i++)
					{
						const int32* WP = HashMapRef.Find(PCGEx::GH2(InMesh.GetVertex(i), CWTolerance));
						if (WP)
						{
							const int32 PointIndex = *WP;
							InMesh.SetVertex(i, Transform.InverseTransformPosition(InTransforms[PointIndex].GetLocation()));
							//InMesh.SetVertexNormal()
							ElemIDs[i] = Colors->AppendElement(FVector4f(InColors[PointIndex]));
						}
						else
						{
							ElemIDs[i] = Colors->AppendElement(DefaultVertexColor);
						}
					}

					for (int32 TriangleID : InMesh.TriangleIndicesItr())
					{
						UE::Geometry::FIndex3i Triangle = InMesh.GetTriangle(TriangleID);
						MaterialID->SetValue(TriangleID, 0);
						Colors->SetTriangle(TriangleID, UE::Geometry::FIndex3i(ElemIDs[Triangle.A], ElemIDs[Triangle.B], ElemIDs[Triangle.C]));
					}
				}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, true);

			Settings->Topology.PostProcessMesh(GetInternalMesh());
		}
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API TBatch : public PCGExClusterMT::TBatch<T>
	{
	protected:
		const FVector2D CWTolerance = FVector2D(1 / 0.001);

		using PCGExClusterMT::TBatch<T>::SharedThis;
		using PCGExClusterMT::TBatch<T>::AsyncManager;

		using PCGExClusterMT::TBatch<T>::ExecutionContext;
		using PCGExClusterMT::TBatch<T>::NodeIndexLookup;

		TSharedPtr<TMap<uint32, int32>> ProjectedHashMap;

	public:
		using PCGExClusterMT::TBatch<T>::VtxDataFacade;

		TBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges):
			PCGExClusterMT::TBatch<T>(InContext, InVtx, InEdges)
		{
			ProjectedHashMap = MakeShared<TMap<uint32, int32>>();
			ProjectedHashMap->Reserve(InVtx->GetNum());
			static_cast<FPCGExTopologyEdgesProcessorContext*>(InContext)->HashMaps[InVtx->IOIndex] = ProjectedHashMap;
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

		virtual void Output() override
		{
			if (!this->bIsBatchValid) { return; }

			PCGEX_TYPED_CONTEXT_AND_SETTINGS(TopologyEdgesProcessor)
			PCGExClusterMT::TBatch<T>::Output();
		}

	protected:
		virtual void OnInitialPostProcess() override
		{
			const int32 NumVtx = VtxDataFacade->GetNum();

			TMap<uint32, int32>& MP = *ProjectedHashMap;
			const TArray<FVector2D>& PP = *this->ProjectedVtxPositions.Get();

			for (int i = 0; i < NumVtx; i++) { MP.Add(PCGEx::GH2(PP[i], CWTolerance), i); }

			PCGExClusterMT::TBatch<T>::OnInitialPostProcess();
		}
	};
}
