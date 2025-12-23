// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExClustersProcessor.h"
#include "PCGExSanitizeClusters.generated.h"


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="clusters/sanitize"))
class UPCGExSanitizeClustersSettings : public UPCGExClustersProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SanitizeClusters, "Cluster : Sanitize", "Ensure the input set of vertex and edges outputs clean, interconnected clusters. May create new clusters, but does not creates nor deletes points/edges.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(ClusterOp); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExClustersProcessorSettings interface
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;
	//~End UPCGExClustersProcessorSettings interface

	/** Graph & Edges output properties. Note that pruning isolated points is ignored. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails;
};

struct FPCGExSanitizeClustersContext final : FPCGExClustersProcessorContext
{
	friend class UPCGExSanitizeClustersSettings;
	friend class FPCGExSanitizeClustersElement;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExSanitizeClustersElement final : public FPCGExClustersProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(SanitizeClusters)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExSanitizeClusters
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExSanitizeClustersContext, UPCGExSanitizeClustersSettings>
	{
	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
			bBuildCluster = false;
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
			: TBatch(InContext, InVtx, InEdges)
		{
			this->bRequiresGraphBuilder = true;
		}

		virtual void OnInitialPostProcess() override;
		virtual void Output() override;
	};
}
