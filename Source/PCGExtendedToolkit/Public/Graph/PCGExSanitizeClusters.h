// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExEdgesProcessor.h"

#include "PCGExSanitizeClusters.generated.h"

/**
 * A Base node to process a set of point using GraphParams.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExSanitizeClustersSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SanitizeClusters, "Graph : Sanitize Clusters", "Ensure the input set of vertex and edges outputs clean, interconnected clusters. May create new clusters, but does not creates nor deletes points/edges.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorGraph; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExEdgesProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End UPCGExEdgesProcessorSettings interface

	/** Graph & Edges output properties. Note that pruning isolated points is ignored. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties, DisplayName="Graph Output Settings"))
	FPCGExGraphBuilderSettings GraphBuilderSettings;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSanitizeClustersContext : public FPCGExEdgesProcessorContext
{
	friend class UPCGExSanitizeClustersSettings;
	friend class FPCGExSanitizeClustersElement;

	virtual ~FPCGExSanitizeClustersContext() override;

	TArray<PCGExGraph::FIndexedEdge> IndexedEdges;

	FPCGExGraphBuilderSettings GraphBuilderSettings;
	PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSanitizeClustersElement : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
