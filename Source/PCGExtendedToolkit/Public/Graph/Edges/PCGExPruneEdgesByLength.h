// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExPruneEdgesByLength.generated.h"

UCLASS(MinimalAPI, BlueprintType, Deprecated, ClassGroup = (Procedural), Category="PCGEx|Graph", meta = (DeprecatedNode, DeprecationMessage = "Prune by Length has been deprecated and should be replaced. See https://nebukam.github.io/PCGExtendedToolkit/doc-clusters/clusters-prune-by-length.html."))
class /*PCGEXTENDEDTOOLKIT_API*/ UDEPRECATED_PCGExPruneEdgesByLengthSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PruneEdgesByLength, "Cluster : Prune edges by Length", "Prune edges by length safely based on edges metrics. For more advanced/involved operations, prune edges yourself and use Sanitize Clusters.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorEdge; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExEdgesProcessorSettings interface
public:
	//~End UPCGExEdgesProcessorSettings interface
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPruneEdgesByLengthContext final : public FPCGExEdgesProcessorContext
{
	friend class UDEPRECATED_PCGExPruneEdgesByLengthSettings;
	friend class FPCGExPruneEdgesByLengthElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPruneEdgesByLengthElement final : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
