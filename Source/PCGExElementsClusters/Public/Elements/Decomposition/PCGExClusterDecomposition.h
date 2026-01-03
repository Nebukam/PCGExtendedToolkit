// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSimpleConvexDecomposer.h"

#include "Core/PCGExClusterMT.h"
#include "Core/PCGExClustersProcessor.h"
#include "PCGExClusterDecomposition.generated.h"

class FPCGExVtxPropertyOperation;
class UPCGExVtxPropertyFactoryData;

namespace PCGExData
{
	template <typename T>
	class TBuffer;
}

namespace PCGExConvexDecomposition
{
	class FBatch;
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="metadata/vtx-properties"))
class UPCGExClusterDecompositionSettings : public UPCGExClustersProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ConvexDecomposition, "Cluster : Decomposition", "Compute convex/k decomposition of clusters and write partition as an ID on the nodes.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(NeighborSampler); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

	/** Write normal from edges on vertices. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExConvexDecompositionDetails DecompositionSettings;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName CellIDAttributeName = FName("CellID");

private:
	friend class FPCGExClusterDecompositionElement;
};

struct FPCGExClusterDecompositionContext final : FPCGExClustersProcessorContext
{
	friend class FPCGExClusterDecompositionElement;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExClusterDecompositionElement final : public FPCGExClustersProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ClusterDecomposition)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExClusterDecomposition
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExClusterDecompositionContext, UPCGExClusterDecompositionSettings>
	{
		friend class FBatch;

		TSharedPtr<PCGExData::TBuffer<int32>> CellIDBuffer;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		virtual void CompleteWork() override;
		virtual void Cleanup() override;
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		TSharedPtr<PCGExData::TBuffer<int32>> CellIDBuffer;

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);
		virtual ~FBatch() override;

		virtual void OnProcessingPreparationComplete() override;
		virtual bool PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor) override;
		virtual void Write() override;
	};
}
