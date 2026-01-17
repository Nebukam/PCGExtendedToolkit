// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExClustersProcessor.h"
#include "Core/PCGExValencyCommon.h"
#include "Core/PCGExValencyBondingRules.h"
#include "Core/PCGExValencyOrbitalSet.h"
#include "Core/PCGExValencySolverOperation.h"

#include "PCGExValencyStaging.generated.h"

class UPCGExValencySolverInstancedFactory;

/**
 * Valency Staging - WFC-like asset staging for cluster nodes.
 * Uses orbital-based compatibility rules to place modules on cluster vertices.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Valency", meta=(Keywords = "wfc wave function collapse valency staging", PCGExNodeLibraryDoc="valency/valency-staging"))
class PCGEXELEMENTSVALENCY_API UPCGExValencyStagingSettings : public UPCGExClustersProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UObject
	virtual void PostInitProperties() override;
	//~End UObject

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ValencyStaging, "Valency Staging", "WFC-like asset staging on cluster vertices using orbital-based compatibility rules.");
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

	/** The bonding rules data asset containing module configurations */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UPCGExValencyBondingRules> BondingRules;

	/** Orbital set - determines which layer's orbital data to read (PCGEx/Valency/Mask/{LayerName}, PCGEx/Valency/Idx/{LayerName}) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UPCGExValencyOrbitalSet> OrbitalSet;

	/** Solver algorithm. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExValencySolverInstancedFactory> Solver;

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
	bool bQuietMissingBondingRules = false;
};

struct PCGEXELEMENTSVALENCY_API FPCGExValencyStagingContext final : FPCGExClustersProcessorContext
{
	friend class FPCGExValencyStagingElement;

	virtual void RegisterAssetDependencies() override;

	TObjectPtr<UPCGExValencyBondingRules> BondingRules;
	TObjectPtr<UPCGExValencyOrbitalSet> OrbitalSet;

	/** Solver factory (registered from settings) */
	UPCGExValencySolverInstancedFactory* Solver = nullptr;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class PCGEXELEMENTSVALENCY_API FPCGExValencyStagingElement final : public FPCGExClustersProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ValencyStaging)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual void PostLoadAssetsDependencies(FPCGExContext* InContext) const override;
	virtual bool PostBoot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExValencyStaging
{
	class FBatch;

	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExValencyStagingContext, UPCGExValencyStagingSettings>
	{
		friend class FBatch;

	protected:
		/** Solver instance */
		TSharedPtr<FPCGExValencySolverOperation> Solver;

		/** Valency states for solver input/output */
		TArray<PCGExValency::FValencyState> ValencyStates;

		/** Attribute writers (owned by batch, forwarded via PrepareSingle) */
		TSharedPtr<PCGExData::TBuffer<int32>> ModuleIndexWriter;
		TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> AssetPathWriter;
		TSharedPtr<PCGExData::TBuffer<bool>> UnsolvableWriter;

		/** Orbital mask reader (vertex attribute) */
		TSharedPtr<PCGExData::TBuffer<int64>> OrbitalMaskReader;

		/** Edge orbital indices reader (edge attribute) */
		TSharedPtr<PCGExData::TBuffer<int64>> EdgeIndicesReader;

		/** Solve result */
		PCGExValency::FSolveResult SolveResult;

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
		/** Build valency states from cluster data */
		void BuildValencyStates();

		/** Run the solver */
		void RunSolver();

		/** Write results to point attributes */
		void WriteResults();
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		/** Orbital mask reader - vertex attribute (owned here, shared with processors) */
		TSharedPtr<PCGExData::TBuffer<int64>> OrbitalMaskReader;

		/** Edge orbital indices reader - edge attribute (owned here, shared with processors) */
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
