// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExClustersProcessor.h"
#include "Core/PCGExValencyOrbitalSet.h"
#include "Data/PCGExData.h"

#include "PCGExWriteValencyOrbitals.generated.h"

namespace PCGExData
{
	template <typename T>
	class TArrayBuffer;
}

/**
 * Writes Valency orbital data to cluster vertices and edges.
 * - Vertex: Orbital mask (int64) at PCGEx/Valency/Mask/{LayerName}
 * - Edge: Packed orbital indices (int64) at PCGEx/Valency/Idx/{LayerName}
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Valency", meta=(Keywords = "valency orbitals write state", PCGExNodeLibraryDoc="valency/write-valency-orbitals"))
class PCGEXELEMENTSVALENCY_API UPCGExWriteValencyOrbitalsSettings : public UPCGExClustersProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(WriteValencyOrbitals, "Valency : Write Orbitals", "Computes and writes orbital masks and indices for Valency solving.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(MiscWrite); }
	
	virtual bool CanDynamicallyTrackKeys() const override { return true; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	virtual bool SupportsDataStealing() const override { return true; }

public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

	/** The orbital set defining layer name, orbitals, and matching parameters */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UPCGExValencyOrbitalSet> OrbitalSet;

	/** If enabled, will output warnings for edges that don't match any orbital */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Warnings", meta=(PCG_NotOverridable))
	bool bWarnOnNoMatch = true;

	/** Quiet mode - suppress missing orbital set errors */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta=(PCG_NotOverridable))
	bool bQuietMissingOrbitalSet = false;

private:
	friend class FPCGExWriteValencyOrbitalsElement;
};

struct PCGEXELEMENTSVALENCY_API FPCGExWriteValencyOrbitalsContext final : FPCGExClustersProcessorContext
{
	friend class FPCGExWriteValencyOrbitalsElement;

	virtual void RegisterAssetDependencies() override;

	TObjectPtr<UPCGExValencyOrbitalSet> OrbitalSet;

	/** Cached orbital data for fast lookup during processing */
	PCGExValency::FOrbitalDirectionResolver OrbitalResolver;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class PCGEXELEMENTSVALENCY_API FPCGExWriteValencyOrbitalsElement final : public FPCGExClustersProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(WriteValencyOrbitals)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual void PostLoadAssetsDependencies(FPCGExContext* InContext) const override;
	virtual bool PostBoot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExWriteValencyOrbitals
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExWriteValencyOrbitalsContext, UPCGExWriteValencyOrbitalsSettings>
	{
		friend class FBatch;

		TSharedPtr<TArray<int64>> VertexMasks;
		TSharedPtr<PCGExData::TBuffer<int64>> IdxWriter;

		/** Count of edges with no orbital match (for warning) */
		int32 NoMatchCount = 0;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override = default;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		virtual void ProcessNodes(const PCGExMT::FScope& Scope) override;
		virtual void OnNodesProcessingComplete() override;

	protected:
		/** Process a single node - compute orbital mask and edge indices */
		void ProcessSingleNode(PCGExClusters::FNode& Node);
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		/** Vertex orbital masks (shared with processors) */
		TSharedPtr<TArray<int64>> VertexMasks;

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);
		virtual ~FBatch() override;

		virtual void OnProcessingPreparationComplete() override;
		virtual bool PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor) override;
		virtual void CompleteWork() override;
	};
}
