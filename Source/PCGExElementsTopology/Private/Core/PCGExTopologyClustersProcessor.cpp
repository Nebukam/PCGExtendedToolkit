// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExTopologyClustersProcessor.h"

#include "CoreMinimal.h"
#include "Core/PCGExContext.h"
#include "PCGComponent.h"
#include "Data/PCGDynamicMeshData.h"
#include "Core/PCGExClusterMT.h"
#include "Core/PCGExClustersProcessor.h"
#include "Core/PCGExPointFilter.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Async/ParallelFor.h"
#include "Data/PCGExData.h"
#include "Clusters/PCGExCluster.h"
#include "Components/PCGExDynamicMeshComponent.h"
#include "Data/Descriptors/PCGExDynamicMeshDescriptor.h"

#define LOCTEXT_NAMESPACE "TopologyProcessor"
#define PCGEX_NAMESPACE TopologyProcessor

PCGExData::EIOInit UPCGExTopologyClustersProcessorSettings::GetMainOutputInitMode() const { return OutputMode == EPCGExTopologyOutputMode::Legacy ? PCGExData::EIOInit::Forward : PCGExData::EIOInit::NoInit; }
PCGExData::EIOInit UPCGExTopologyClustersProcessorSettings::GetEdgeOutputInitMode() const { return OutputMode == EPCGExTopologyOutputMode::Legacy ? PCGExData::EIOInit::Forward : PCGExData::EIOInit::NoInit; }

TArray<FPCGPinProperties> UPCGExTopologyClustersProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGExClusters::Labels::SourceHolesLabel, "Omit cells that contain any points from this dataset", Normal)
	if (SupportsEdgeConstraints())
	{
		PCGEX_PIN_FILTERS(PCGExClusters::Labels::SourceEdgeConstrainsFiltersLabel, "Constrained edges filters.", Normal)
	}
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExTopologyClustersProcessorSettings::OutputPinProperties() const
{
	if (OutputMode == EPCGExTopologyOutputMode::Legacy) { return Super::OutputPinProperties(); }

	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_MESH(PCGExTopology::Labels::OutputMeshLabel, "PCG Dynamic Mesh", Normal)
	return PinProperties;
}

#if WITH_EDITOR
void UPCGExTopologyClustersProcessorSettings::ApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins)
{
	for (TObjectPtr<UPCGPin>& OutPin : OutputPins)
	{
		// If vtx/edge pins are connected, set Legacy output mode
		if ((OutPin->Properties.Label == PCGExClusters::Labels::OutputVerticesLabel || OutPin->Properties.Label == PCGExClusters::Labels::OutputEdgesLabel) && OutPin->EdgeCount() > 0) { OutputMode = EPCGExTopologyOutputMode::Legacy; }
	}
	Super::ApplyDeprecationBeforeUpdatePins(InOutNode, InputPins, OutputPins);
}
#endif

void FPCGExTopologyClustersProcessorContext::RegisterAssetDependencies()
{
	PCGEX_SETTINGS_LOCAL(TopologyClustersProcessor)

	FPCGExClustersProcessorContext::RegisterAssetDependencies();

	if (Settings->Topology.Material.ToSoftObjectPath().IsValid())
	{
		AddAssetDependency(Settings->Topology.Material.ToSoftObjectPath());
	}
}

bool FPCGExTopologyClustersProcessorElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClustersProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(TopologyClustersProcessor)

	Context->HolesFacade = PCGExData::TryGetSingleFacade(Context, PCGExClusters::Labels::SourceHolesLabel, false, false);
	if (Context->HolesFacade && Settings->ProjectionDetails.Method == EPCGExProjectionMethod::Normal)
	{
		Context->Holes = MakeShared<PCGExClusters::FHoles>(Context, Context->HolesFacade.ToSharedRef(), Settings->ProjectionDetails);
	}

	PCGExArrayHelpers::AppendUniqueEntriesFromCommaSeparatedList(Settings->CommaSeparatedComponentTags, Context->ComponentTags);

	GetInputFactories(Context, PCGExClusters::Labels::SourceEdgeConstrainsFiltersLabel, Context->EdgeConstraintsFilterFactories, PCGExFactories::ClusterEdgeFilters, false);

	Context->HashMaps.Init(nullptr, Context->MainPoints->Num());
	return true;
}

namespace PCGExTopologyEdges
{
	IProcessor::IProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
		: PCGExClusterMT::IProcessor(InVtxDataFacade, InEdgeDataFacade)
	{
		DefaultEdgeFilterValue = false;
	}

	void IProcessor::InitConstraints()
	{
	}

	TSharedPtr<PCGExClusters::FCluster> IProcessor::HandleCachedCluster(const TSharedRef<PCGExClusters::FCluster>& InClusterRef)
	{
		// Create a light working copy with nodes only, will be deleted.
		return MakeShared<PCGExClusters::FCluster>(InClusterRef, VtxDataFacade->Source, EdgeDataFacade->Source, NodeIndexLookup, true, false, false);
	}

	bool IProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		FPCGExTopologyClustersProcessorContext* Context = static_cast<FPCGExTopologyClustersProcessorContext*>(ExecutionContext);
		const UPCGExTopologyClustersProcessorSettings* Settings = ExecutionContext->GetInputSettings<UPCGExTopologyClustersProcessorSettings>();

		EdgeDataFacade->bSupportsScopedGet = true;
		EdgeFilterFactories = &Context->EdgeConstraintsFilterFactories;

		ProjectedHashMap = Context->HashMaps[VtxDataFacade->Source->IOIndex];

		if (!PCGExClusterMT::IProcessor::Process(InTaskManager)) { return false; }

		if (Context->HolesFacade) { Holes = Context->Holes ? Context->Holes : MakeShared<PCGExClusters::FHoles>(Context, Context->HolesFacade.ToSharedRef(), this->ProjectionDetails); }

		UVDetails = Settings->Topology.UVChannels;
		UVDetails.Prepare(VtxDataFacade);

		bIsPreviewMode = ExecutionContext->GetComponent()->IsInPreviewMode();

		CellsConstraints = MakeShared<PCGExClusters::FCellConstraints>(Settings->Constraints);
		CellsConstraints->Reserve(Cluster->Edges->Num());

		if (Settings->Constraints.bOmitWrappingBounds) { CellsConstraints->BuildWrapperCell(Cluster.ToSharedRef(), *this->ProjectedVtxPositions.Get()); }
		CellsConstraints->Holes = Holes;

		InitConstraints();

		for (PCGExClusters::FNode& Node : *Cluster->Nodes) { Node.bValid = false; } // Invalidate all edges, triangulation will mark valid nodes to rebuild an index

		// IMPORTANT : Need to wait for projection to be completed.
		// Children should start work only in CompleteWork!!

		if (Settings->OutputMode == EPCGExTopologyOutputMode::PCGDynamicMesh)
		{
			InternalMeshData = Context->ManagedObjects->New<UPCGDynamicMeshData>();
			if (!InternalMeshData) { return false; }
		}

		InternalMesh = Context->ManagedObjects->New<UDynamicMesh>();
		InternalMesh->InitializeMesh();

		if (InternalMeshData)
		{
			InternalMeshData->Initialize(InternalMesh, true);
			InternalMesh = InternalMeshData->GetMutableDynamicMesh();
			if (UMaterialInterface* Material = Settings->Topology.Material.Get()) { InternalMeshData->SetMaterials({Material}); }
		}

		return true;
	}

	void IProcessor::Output()
	{
		if (!this->bIsProcessorValid) { return; }

		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExPathSplineMesh::FProcessor::Output);

		FPCGExTopologyClustersProcessorContext* Context = static_cast<FPCGExTopologyClustersProcessorContext*>(ExecutionContext);
		const UPCGExTopologyClustersProcessorSettings* Settings = ExecutionContext->GetInputSettings<UPCGExTopologyClustersProcessorSettings>();

		if (InternalMeshData)
		{
			TSet<FString> MeshTags;

			EdgeDataFacade->Source->Tags->DumpTo(MeshTags);
			VtxDataFacade->Source->Tags->DumpTo(MeshTags);

			Context->StageOutput(InternalMeshData, PCGExTopology::MeshOutputLabel, PCGExData::EStaging::Managed, MeshTags);

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
		Context->GetMutableComponent()->IgnoreChangeOriginDuringGenerationWithScope(DynamicMeshComponent, [&]()
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

	void IProcessor::Cleanup()
	{
		PCGExClusterMT::IProcessor::Cleanup();
		CellsConstraints->Cleanup();
	}

	void IProcessor::FilterConstrainedEdgeScope(const PCGExMT::FScope& Scope)
	{
		int32 LocalConstrainedEdgesNum = 0;
		PCGEX_SCOPE_LOOP(i) { if (EdgeFilterCache[i]) { LocalConstrainedEdgesNum++; } }
		FPlatformAtomics::InterlockedAdd(&ConstrainedEdgesNum, LocalConstrainedEdgesNum);
	}

	void IProcessor::ApplyPointData()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TopologyClustersProcessor::ApplyPointData);

		FPCGExTopologyClustersProcessorContext* Context = static_cast<FPCGExTopologyClustersProcessorContext*>(ExecutionContext);
		const UPCGExTopologyClustersProcessorSettings* Settings = ExecutionContext->GetInputSettings<UPCGExTopologyClustersProcessorSettings>();

		FTransform Transform = Settings->OutputMode == EPCGExTopologyOutputMode::PCGDynamicMesh ? Context->GetComponent()->GetOwner()->GetTransform() : FTransform::Identity;
		Transform.SetScale3D(FVector::OneVector);
		Transform.SetRotation(FQuat::Identity);

		InternalMesh->EditMesh([&](FDynamicMesh3& InMesh)
		{
			const int32 VtxCount = InMesh.MaxVertexID();
			const TConstPCGValueRange<FTransform> InTransforms = VtxDataFacade->GetIn()->GetConstTransformValueRange();
			const TConstPCGValueRange<FVector4> InColors = VtxDataFacade->GetIn()->GetConstColorValueRange();
			const TMap<uint64, int32>& HashMapRef = *ProjectedHashMap;

			FVector4f DefaultVertexColor = FVector4f(Settings->Topology.DefaultVertexColor);

			InMesh.EnableAttributes();
			InMesh.Attributes()->EnablePrimaryColors();
			InMesh.Attributes()->EnableMaterialID();

			UE::Geometry::FDynamicMeshColorOverlay* Colors = InMesh.Attributes()->PrimaryColors();
			UE::Geometry::FDynamicMeshMaterialAttribute* MaterialID = InMesh.Attributes()->GetMaterialID();

			TArray<int32> VtxIDs;
			VtxIDs.Init(-1, VtxCount);

			TArray<int32> ElemIDs;
			ElemIDs.SetNum(VtxCount);

			for (int32 i = 0; i < VtxCount; i++) { ElemIDs[i] = Colors->AppendElement(DefaultVertexColor); }

			ParallelFor(VtxCount, [&](int32 i)
			{
				const int32* WP = HashMapRef.Find(PCGEx::GH2(InMesh.GetVertex(i), CWTolerance));
				if (WP)
				{
					const int32 PointIndex = *WP;
					VtxIDs[i] = PointIndex;
					InMesh.SetVertex(i, Transform.InverseTransformPosition(InTransforms[PointIndex].GetLocation()));
					Colors->SetElement(ElemIDs[i], FVector4f(InColors[PointIndex]));
				}
			});

			TArray<int32> TriangleIDs;
			TriangleIDs.Reserve(InMesh.TriangleCount());
			for (int32 TriangleID : InMesh.TriangleIndicesItr())
			{
				TriangleIDs.Add(TriangleID);

				const UE::Geometry::FIndex3i Triangle = InMesh.GetTriangle(TriangleID);
				MaterialID->SetValue(TriangleID, 0);
				Colors->SetTriangle(TriangleID, UE::Geometry::FIndex3i(ElemIDs[Triangle.A], ElemIDs[Triangle.B], ElemIDs[Triangle.C]));
			}

			UVDetails.Write(TriangleIDs, VtxIDs, InMesh);
		}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, true);

		Settings->Topology.PostProcessMesh(GetInternalMesh());
	}

	IBatch::IBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
		: PCGExClusterMT::IBatch(InContext, InVtx, InEdges)
	{
		ProjectedHashMap = MakeShared<TMap<uint64, int32>>();
		ProjectedHashMap->Reserve(InVtx->GetNum());
		static_cast<FPCGExTopologyClustersProcessorContext*>(InContext)->HashMaps[InVtx->IOIndex] = ProjectedHashMap;
	}

	void IBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		PCGExClusterMT::IBatch::RegisterBuffersDependencies(FacadePreloader);

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(TopologyClustersProcessor)

		check(Settings);

		Settings->Topology.UVChannels.RegisterBuffersDependencies(ExecutionContext, FacadePreloader);

		if (Settings->SupportsEdgeConstraints())
		{
			PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->EdgeConstraintsFilterFactories, FacadePreloader);
		}
	}

	void IBatch::Output()
	{
		if (!this->bIsBatchValid) { return; }

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(TopologyClustersProcessor)
		PCGExClusterMT::IBatch::Output();
	}

	void IBatch::OnInitialPostProcess()
	{
		const int32 NumVtx = VtxDataFacade->GetNum();

		TMap<uint64, int32>& MP = *ProjectedHashMap;
		const TArray<FVector2D>& PP = *this->ProjectedVtxPositions.Get();

		for (int i = 0; i < NumVtx; i++) { MP.Add(PCGEx::GH2(PP[i], CWTolerance), i); }

		PCGExClusterMT::IBatch::OnInitialPostProcess();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
