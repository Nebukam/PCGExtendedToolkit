// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExClusterMT.h"
#include "PCGExPointsProcessor.h"

#include "PCGExEdgesProcessor.generated.h"

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgesProcessorSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(EdgesProcessorSettings, "Edges Processor Settings", "TOOLTIP_TEXT");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorEdge; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const;

	virtual FName GetMainInputLabel() const override { return PCGExGraph::SourceVerticesLabel; }
	virtual FName GetMainOutputLabel() const override { return PCGExGraph::OutputVerticesLabel; }

	virtual bool GetMainAcceptMultipleData() const override;
	//~End UPCGExPointsProcessorSettings
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExEdgesProcessorContext : public FPCGExPointsProcessorContext
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
	TMap<uint32, int32> EndpointsLookup;
	TArray<int32> EndpointsAdjacency;

	virtual bool AdvancePointsIO(const bool bCleanupKeys = true) override;
	virtual bool AdvanceEdges(const bool bBuildCluster, const bool bCleanupKeys = true); // Advance edges within current points

	PCGExCluster::FCluster* CurrentCluster = nullptr;

	void OutputPointsAndEdges() const;

	template <class InitializeFunc, class LoopBodyFunc>
	bool ProcessCurrentEdges(InitializeFunc&& Initialize, LoopBodyFunc&& LoopBody, bool bForceSync = false) { return Process(Initialize, LoopBody, CurrentEdges->GetNum(), bForceSync); }

	template <class LoopBodyFunc>
	bool ProcessCurrentEdges(LoopBodyFunc&& LoopBody, bool bForceSync = false) { return Process(LoopBody, CurrentEdges->GetNum(), bForceSync); }

	template <class InitializeFunc, class LoopBodyFunc>
	bool ProcessCurrentCluster(InitializeFunc&& Initialize, LoopBodyFunc&& LoopBody, bool bForceSync = false) { return Process(Initialize, LoopBody, CurrentCluster->Nodes->Num(), bForceSync); }

	template <class LoopBodyFunc>
	bool ProcessCurrentCluster(LoopBodyFunc&& LoopBody, bool bForceSync = false) { return Process(LoopBody, CurrentCluster->Nodes->Num(), bForceSync); }

	FPCGExGraphBuilderDetails GraphBuilderDetails;

	bool bWaitingOnClusterProjection = false;

	int32 GetClusterProcessorsNum() const
	{
		int32 Num = 0;
		for (const PCGExClusterMT::FClusterProcessorBatchBase* Batch : Batches) { Num += Batch->GetNumProcessors(); }
		return Num;
	}

	template<typename T>
	void GatherClusterProcessors(TArray<T*> OutProcessors)
	{
		OutProcessors.Reserve(GetClusterProcessorsNum());
		for (const PCGExClusterMT::FClusterProcessorBatchBase* Batch : Batches)
		{
			const PCGExClusterMT::TBatch<T>* TypedBatch = static_cast<const PCGExClusterMT::TBatch<T>*>(Batch); 
			OutProcessors.Append(TypedBatch->Processors);
		}
	}
	
	void OutputBatches() const
	{
		for (PCGExClusterMT::FClusterProcessorBatchBase* Batch : Batches) { Batch->Output(); }
	}

protected:
	virtual bool ProcessClusters();

	TArray<PCGExClusterMT::FClusterProcessorBatchBase*> Batches;

	bool bHasValidHeuristics = false;

	PCGExMT::AsyncState TargetState_ClusterProcessingDone;
	bool bDoClusterBatchGraphBuilding = false;
	bool bDoClusterBatchWritingStep = false;

	bool bClusterRequiresHeuristics = false;
	bool bClusterBatchInlined = false;
	int32 CurrentBatchIndex = -1;
	PCGExClusterMT::FClusterProcessorBatchBase* CurrentBatch = nullptr;


	template <typename T, class ValidateEntriesFunc, class InitBatchFunc>
	bool StartProcessingClusters(ValidateEntriesFunc&& ValidateEntries, InitBatchFunc&& InitBatch, const PCGExMT::AsyncState InState, const bool bInlined = false)
	{
		ResetAsyncWork();

		PCGEX_DELETE_TARRAY(Batches)

		bClusterBatchInlined = bInlined;
		CurrentBatchIndex = -1;
		TargetState_ClusterProcessingDone = InState;

		bClusterRequiresHeuristics = true;
		bDoClusterBatchGraphBuilding = false;
		bDoClusterBatchWritingStep = false;
		bBuildEndpointsLookup = false;

		Batches.Reserve(MainPoints->Pairs.Num());

		while (AdvancePointsIO(false))
		{
			if (!TaggedEdges)
			{
				PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("Some input points have no bound edges."));
				continue;
			}

			if (!ValidateEntries(TaggedEdges)) { continue; }

			T* NewBatch = new T(this, CurrentIO, TaggedEdges->Entries);
			InitBatch(NewBatch);

			if (NewBatch->RequiresHeuristics())
			{
				bClusterRequiresHeuristics = true;

				if (!bHasValidHeuristics)
				{
					PCGEX_DELETE(NewBatch)
					continue;
				}
			}

			if (NewBatch->bRequiresWriteStep)
			{
				bDoClusterBatchWritingStep = true;
			}

			NewBatch->EdgeCollection = MainEdges;

			if (NewBatch->RequiresGraphBuilder())
			{
				bDoClusterBatchGraphBuilding = true;
				NewBatch->GraphBuilderDetails = GraphBuilderDetails;
			}

			Batches.Add(NewBatch);
			if (!bClusterBatchInlined) { PCGExClusterMT::ScheduleBatch(GetAsyncManager(), NewBatch); }
		}

		if (Batches.IsEmpty()) { return false; }

		if (bClusterBatchInlined) { AdvanceBatch(); }
		else { SetAsyncState(PCGExClusterMT::MTState_ClusterProcessing); }
		return true;
	}

	virtual void OnBatchesProcessingDone()
	{
	}

	virtual void OnBatchesCompletingWorkDone()
	{
	}

	virtual void OnBatchesCompilationDone(bool bWritten)
	{
	}

	virtual void OnBatchesWritingDone()
	{
	}

	bool HasValidHeuristics() const;

	void AdvanceBatch();

	int32 CurrentEdgesIndex = -1;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExEdgesProcessorElement : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

	virtual void DisabledPassThroughData(FPCGContext* Context) const override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual FPCGContext* InitializeContext(
		FPCGExPointsProcessorContext* InContext,
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) const override;
};
