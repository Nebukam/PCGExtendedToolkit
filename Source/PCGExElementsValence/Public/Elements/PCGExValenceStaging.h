// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExClustersProcessor.h"
#include "Core/PCGExValenceCommon.h"
#include "Core/PCGExValenceRuleset.h"
#include "Core/PCGExValenceSocketCollection.h"
#include "Core/PCGExValenceSolverOperation.h"

#include "PCGExValenceStaging.generated.h"

class UPCGExValenceSolverInstancedFactory;

/**
 * Valence Staging - WFC-like asset staging for cluster nodes.
 * Uses socket-based compatibility rules to place modules on cluster vertices.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Valence", meta=(Keywords = "wfc wave function collapse valence staging", PCGExNodeLibraryDoc="valence/valence-staging"))
class PCGEXELEMENTSVALENCE_API UPCGExValenceStagingSettings : public UPCGExClustersProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UObject
	virtual void PostInitProperties() override;
	//~End UObject

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ValenceStaging, "Valence Staging", "WFC-like asset staging on cluster vertices using socket-based compatibility rules.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(MiscAdd); }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

	/** The ruleset data asset containing module configurations */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UPCGExValenceRuleset> Ruleset;

	/** Socket collection - determines which layer's socket data to read (PCGEx/Valence/Mask/{LayerName}, PCGEx/Valence/Idx/{LayerName}) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UPCGExValenceSocketCollection> SocketCollection;

	/** Solver algorithm. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExValenceSolverInstancedFactory> Solver;

	/** If enabled, use the point's seed attribute to vary per-cluster solving */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bUsePerClusterSeed = false;

	/** Attribute name for the resolved module index output */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	FName ModuleIndexAttributeName = FName("ModuleIndex");

	/** Attribute name for the resolved asset path output */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	FName AssetPathAttributeName = FName("AssetPath");

	/** If enabled, output an attribute marking unsolvable nodes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bOutputUnsolvableMarker = true;

	/** Attribute name for the unsolvable marker */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bOutputUnsolvableMarker"))
	FName UnsolvableAttributeName = FName("bUnsolvable");

	/** If enabled, prune nodes that failed to solve */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bPruneUnsolvable = false;

	/** Warnings and errors */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta=(PCG_NotOverridable))
	bool bQuietMissingRuleset = false;
};

struct PCGEXELEMENTSVALENCE_API FPCGExValenceStagingContext final : FPCGExClustersProcessorContext
{
	friend class FPCGExValenceStagingElement;

	virtual void RegisterAssetDependencies() override;

	TObjectPtr<UPCGExValenceRuleset> Ruleset;
	TObjectPtr<UPCGExValenceSocketCollection> SocketCollection;

	/** Solver factory (registered from settings) */
	UPCGExValenceSolverInstancedFactory* Solver = nullptr;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class PCGEXELEMENTSVALENCE_API FPCGExValenceStagingElement final : public FPCGExClustersProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ValenceStaging)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual void PostLoadAssetsDependencies(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExValenceStaging
{
	class FBatch;

	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExValenceStagingContext, UPCGExValenceStagingSettings>
	{
		friend class FBatch;

	protected:
		/** Solver instance */
		TSharedPtr<FPCGExValenceSolverOperation> Solver;

		/** Node slots for solver input/output */
		TArray<PCGExValence::FNodeSlot> NodeSlots;

		/** Attribute writers (owned by batch, forwarded via PrepareSingle) */
		TSharedPtr<PCGExData::TBuffer<int32>> ModuleIndexWriter;
		TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> AssetPathWriter;
		TSharedPtr<PCGExData::TBuffer<bool>> UnsolvableWriter;

		/** Socket mask reader (vertex attribute) */
		TSharedPtr<PCGExData::TBuffer<int64>> SocketMaskReader;

		/** Edge socket indices reader (edge attribute) */
		TSharedPtr<PCGExData::TBuffer<int64>> EdgeIndicesReader;

		/** Solve result */
		PCGExValence::FSolveResult SolveResult;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override = default;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;

		virtual void ProcessNodes(const PCGExMT::FScope& Scope) override;
		virtual void OnNodesProcessingComplete() override;
		virtual void Write() override;

	protected:
		/** Build node contexts from cluster data */
		void BuildNodeSlots();

		/** Run the solver */
		void RunSolver();

		/** Write results to point attributes */
		void WriteResults();
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		/** Socket mask reader - vertex attribute (owned here, shared with processors) */
		TSharedPtr<PCGExData::TBuffer<int64>> SocketMaskReader;

		/** Edge socket indices reader - edge attribute (owned here, shared with processors) */
		TSharedPtr<PCGExData::TBuffer<int64>> EdgeIndicesReader;

		/** Attribute writers (owned here, shared with processors) */
		TSharedPtr<PCGExData::TBuffer<int32>> ModuleIndexWriter;
		TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> AssetPathWriter;
		TSharedPtr<PCGExData::TBuffer<bool>> UnsolvableWriter;

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);
		virtual ~FBatch() override;

		virtual void OnProcessingPreparationComplete() override;
		virtual bool PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor) override;
		virtual void Write() override;
	};
}
