// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExClustersProcessor.h"
#include "Core/PCGExValenceSocketCollection.h"
#include "Data/PCGExData.h"

#include "PCGExWriteValenceSockets.generated.h"

namespace PCGExData
{
	template <typename T>
	class TArrayBuffer;
}

/**
 * Writes Valence socket data to cluster vertices and edges.
 * - Vertex: Socket mask (int64) at PCGEx/Valence/Mask/{LayerName}
 * - Edge: Packed socket indices (int64) at PCGEx/Valence/Idx/{LayerName}
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Valence", meta=(Keywords = "valence sockets write state", PCGExNodeLibraryDoc="valence/write-valence-sockets"))
class PCGEXELEMENTSVALENCE_API UPCGExWriteValenceSocketsSettings : public UPCGExClustersProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(WriteValenceSockets, "Write Valence Sockets", "Computes and writes socket masks and indices for Valence solving.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(MiscWrite); }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

	/** The socket collection defining layer name, sockets, and matching parameters */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UPCGExValenceSocketCollection> SocketCollection;

	/** If enabled, will output warnings for edges that don't match any socket */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Warnings", meta=(PCG_NotOverridable))
	bool bWarnOnNoMatch = true;

	/** Quiet mode - suppress missing collection errors */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta=(PCG_NotOverridable))
	bool bQuietMissingCollection = false;

private:
	friend class FPCGExWriteValenceSocketsElement;
};

struct PCGEXELEMENTSVALENCE_API FPCGExWriteValenceSocketsContext final : FPCGExClustersProcessorContext
{
	friend class FPCGExWriteValenceSocketsElement;

	virtual void RegisterAssetDependencies() override;

	TObjectPtr<UPCGExValenceSocketCollection> SocketCollection;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class PCGEXELEMENTSVALENCE_API FPCGExWriteValenceSocketsElement final : public FPCGExClustersProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(WriteValenceSockets)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual void PostLoadAssetsDependencies(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExWriteValenceSockets
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExWriteValenceSocketsContext, UPCGExWriteValenceSocketsSettings>
	{
		friend class FBatch;

		TSharedPtr<TArray<int64>> VertexMasks;
		TSharedPtr<PCGExData::TBuffer<int64>> IdxWriter;

		/** Count of edges with no socket match (for warning) */
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
		/** Process a single node - compute socket mask and edge indices */
		void ProcessSingleNode(PCGExClusters::FNode& Node);
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		/** Vertex socket masks (shared with processors) */
		TSharedPtr<TArray<int64>> VertexMasks;

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);
		virtual ~FBatch() override;

		virtual void OnProcessingPreparationComplete() override;
		virtual bool PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor) override;
		virtual void CompleteWork() override;
	};
}
