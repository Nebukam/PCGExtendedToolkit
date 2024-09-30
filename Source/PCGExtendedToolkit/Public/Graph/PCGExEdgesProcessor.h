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

	/** Whether scoped attribute read is enabled or not. Disabling this on small dataset may greatly improve performance. It's enabled by default for legacy reasons. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bScopedIndexLookupBuild = false;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExEdgesProcessorContext : public FPCGExPointsProcessorContext
{
	friend class UPCGExEdgesProcessorSettings;
	friend class FPCGExEdgesProcessorElement;

	virtual ~FPCGExEdgesProcessorContext() override;

	bool bBuildEndpointsLookup = true;

	TSharedPtr<PCGExData::FPointIOCollection> MainEdges;
	TSharedPtr<PCGExData::FPointIO> CurrentEdges;

	TUniquePtr<PCGExData::FPointIOTaggedDictionary> InputDictionary;
	TSharedPtr<PCGExData::FPointIOTaggedEntries> TaggedEdges;
	TMap<uint32, int32> EndpointsLookup;
	TArray<int32> EndpointsAdjacency;

	virtual bool AdvancePointsIO(const bool bCleanupKeys = true) override;
	virtual bool AdvanceEdges(const bool bBuildCluster, const bool bCleanupKeys = true); // Advance edges within current points

	TSharedPtr<PCGExCluster::FCluster> CurrentCluster;

	void OutputPointsAndEdges() const;

	FPCGExGraphBuilderDetails GraphBuilderDetails;

	int32 GetClusterProcessorsNum() const;

	template <typename T>
	void GatherClusterProcessors(TArray<TSharedPtr<T>>& OutProcessors)
	{
		OutProcessors.Reserve(GetClusterProcessorsNum());
		for (const TSharedPtr<PCGExClusterMT::FClusterProcessorBatchBase>& Batch : Batches)
		{
			PCGExClusterMT::TBatch<T>* TypedBatch = static_cast<PCGExClusterMT::TBatch<T>*>(Batch.Get());
			OutProcessors.Append(TypedBatch->Processors);
		}
	}

	void OutputBatches() const;

protected:
	virtual bool ProcessClusters(const PCGExMT::AsyncState NextStateId, const bool bIsNextStateAsync = false);
	virtual bool CompileGraphBuilders(const bool bOutputToContext, const PCGExMT::AsyncState NextStateId);

	TArray<TSharedPtr<PCGExClusterMT::FClusterProcessorBatchBase>> Batches;

	bool bScopedIndexLookupBuild = false;
	bool bHasValidHeuristics = false;

	bool bDoClusterBatchWritingStep = false;

	bool bClusterRequiresHeuristics = false;
	bool bClusterBatchInlined = false;
	int32 CurrentBatchIndex = -1;
	TSharedPtr<PCGExClusterMT::FClusterProcessorBatchBase> CurrentBatch;

	template <typename T, class ValidateEntriesFunc, class InitBatchFunc>
	bool StartProcessingClusters(ValidateEntriesFunc&& ValidateEntries, InitBatchFunc&& InitBatch, const bool bInlined = false)
	{
		ResumeExecution();

		Batches.Empty();

		bClusterBatchInlined = bInlined;
		CurrentBatchIndex = -1;

		bBatchProcessingEnabled = false;
		bClusterRequiresHeuristics = true;
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

			TSharedPtr<T> NewBatch = MakeShared<T>(this, CurrentIO.ToSharedRef(), TaggedEdges->Entries);
			InitBatch(NewBatch);

			if (NewBatch->bRequiresWriteStep)
			{
				bDoClusterBatchWritingStep = true;
			}

			if (NewBatch->RequiresHeuristics())
			{
				bClusterRequiresHeuristics = true;
				if (!bHasValidHeuristics) { continue; }
			}

			NewBatch->EdgeCollection = MainEdges;

			if (NewBatch->RequiresGraphBuilder())
			{
				NewBatch->GraphBuilderDetails = GraphBuilderDetails;
			}

			Batches.Add(NewBatch);
			if (!bClusterBatchInlined) { PCGExClusterMT::ScheduleBatch(GetAsyncManager(), NewBatch, bScopedIndexLookupBuild); }
		}

		if (Batches.IsEmpty()) { return false; }

		bBatchProcessingEnabled = true;
		if (!bClusterBatchInlined) { SetAsyncState(PCGExClusterMT::MTState_ClusterProcessing); }
		return true;
	}

	virtual void ClusterProcessing_InitialProcessingDone()
	{
	}

	virtual void ClusterProcessing_WorkComplete()
	{
	}

	virtual void ClusterProcessing_WritingDone()
	{
	}

	virtual void ClusterProcessing_GraphCompilationDone()
	{
	}

	bool HasValidHeuristics() const;

	void AdvanceBatch(const PCGExMT::AsyncState NextStateId, const bool bIsNextStateAsync);

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
