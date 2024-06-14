// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExClusterMT.h"
#include "PCGExPointsProcessor.h"

#include "PCGExEdgesProcessor.generated.h"

class UPCGExNodeStateFactory;

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExEdgesProcessorSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(EdgesProcessorSettings, "Edges Processor Settings", "TOOLTIP_TEXT");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorEdge; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const;

	virtual bool RequiresDeterministicClusters() const;

	virtual FName GetMainInputLabel() const override;
	virtual FName GetMainOutputLabel() const override;

	virtual FName GetVtxFilterLabel() const;
	virtual FName GetEdgesFilterLabel() const;

	bool SupportsVtxFilters() const;
	bool SupportsEdgesFilters() const;

	virtual bool GetMainAcceptMultipleData() const override;
	//~End UPCGExPointsProcessorSettings interface
};

struct PCGEXTENDEDTOOLKIT_API FPCGExEdgesProcessorContext : public FPCGExPointsProcessorContext
{
	friend class UPCGExEdgesProcessorSettings;
	friend class FPCGExEdgesProcessorElement;

	virtual ~FPCGExEdgesProcessorContext() override;

	bool bDeterministicClusters = false;
	bool bBuildEndpointsLookup = true;

	PCGExData::FPointIOCollection* MainEdges = nullptr;
	PCGExData::FPointIO* CurrentEdges = nullptr;

	PCGExData::FPointIOTaggedDictionary* InputDictionary = nullptr;
	PCGExData::FPointIOTaggedEntries* TaggedEdges = nullptr;
	TMap<int64, int32> EndpointsLookup;
	TArray<int32> EndpointsAdjacency;

	virtual bool ProcessorAutomation() override;
	virtual bool AdvancePointsIO(const bool bCleanupKeys = true) override;
	virtual bool AdvanceEdges(const bool bBuildCluster, const bool bCleanupKeys = true); // Advance edges within current points

	PCGExCluster::FCluster* CurrentCluster = nullptr;
	PCGExCluster::FClusterProjection* ClusterProjection = nullptr;

	bool ProjectCluster();

	void OutputPointsAndEdges();

	template <class InitializeFunc, class LoopBodyFunc>
	bool ProcessCurrentEdges(InitializeFunc&& Initialize, LoopBodyFunc&& LoopBody, bool bForceSync = false) { return Process(Initialize, LoopBody, CurrentEdges->GetNum(), bForceSync); }

	template <class LoopBodyFunc>
	bool ProcessCurrentEdges(LoopBodyFunc&& LoopBody, bool bForceSync = false) { return Process(LoopBody, CurrentEdges->GetNum(), bForceSync); }

	template <class InitializeFunc, class LoopBodyFunc>
	bool ProcessCurrentCluster(InitializeFunc&& Initialize, LoopBodyFunc&& LoopBody, bool bForceSync = false) { return Process(Initialize, LoopBody, CurrentCluster->Nodes.Num(), bForceSync); }

	template <class LoopBodyFunc>
	bool ProcessCurrentCluster(LoopBodyFunc&& LoopBody, bool bForceSync = false) { return Process(LoopBody, CurrentCluster->Nodes.Num(), bForceSync); }

	FPCGExGeo2DProjectionSettings ProjectionSettings;

	bool bWaitingOnClusterProjection = false;

protected:
	bool ProcessFilters();
	bool ProcessClusters();

	TArray<PCGExClusterMT::FClusterProcessorBatchBase*> Batches;

	PCGExMT::AsyncState State_ClusterProcessingDone;
	bool bClusterUseGraphBuilder = false;

	template <typename T, class ValidateEntriesFunc, class InitBatchFunc>
	bool StartProcessingClusters(ValidateEntriesFunc&& ValidateEntries, InitBatchFunc&& InitBatch, const PCGExMT::AsyncState InState)
	{
		State_ClusterProcessingDone = InState;

		bClusterUseGraphBuilder = false;
		bBuildEndpointsLookup = false;

		while (AdvancePointsIO(false))
		{
			if (!TaggedEdges)
			{
				PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("Some input points have no bound edges."));
				continue;
			}

			if (!ValidateEntries(TaggedEdges)) { continue; }

			T* NewBatch = new T(this, CurrentIO, TaggedEdges->Entries);
			Batches.Add(NewBatch);

			NewBatch->EdgeCollection = MainEdges;
			if (VtxFiltersData) { NewBatch->SetVtxFilterData(VtxFiltersData, DefaultVtxFilterResult()); }

			InitBatch(NewBatch);

			if (NewBatch->UseGraphBuilder()) { bClusterUseGraphBuilder = true; }

			PCGExClusterMT::ScheduleBatch(GetAsyncManager(), NewBatch);
		}

		if (Batches.IsEmpty()) { return false; }

		SetAsyncState(PCGExClusterMT::State_WaitingOnClusterProcessing);
		return true;
	}

	int32 CurrentEdgesIndex = -1;

	TArray<int32> VtxIndices;

	virtual bool DefaultVtxFilterResult() const;

	UPCGExNodeStateFactory* VtxFiltersData = nullptr;
	PCGExCluster::FNodeStateHandler* VtxFiltersHandler = nullptr;
	TArray<bool> VtxFilterResults;
	bool bRequireVtxFilterPreparation = false;

	UPCGExNodeStateFactory* EdgesFiltersData = nullptr;
	PCGExCluster::FNodeStateHandler* EdgesFiltersHandler = nullptr;
	TArray<bool> EdgeFilterResults;
	bool bRequireEdgesFilterPreparation = false;

	bool bWaitingOnFilterWork = false;
};

class PCGEXTENDEDTOOLKIT_API FPCGExEdgesProcessorElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

	virtual void DisabledPassThroughData(FPCGContext* Context) const override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual FPCGContext* InitializeContext(
		FPCGExPointsProcessorContext* InContext,
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) const override;
};
