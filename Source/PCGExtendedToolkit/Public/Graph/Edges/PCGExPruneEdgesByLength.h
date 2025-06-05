// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExPruneEdgesByLength.generated.h"

UCLASS(MinimalAPI, BlueprintType, Deprecated, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta = (DeprecatedNode, DeprecationMessage = "Prune by Length has been deprecated and should be replaced. See https://nebukam.github.io/PCGExtendedToolkit/doc-clusters/clusters-prune-by-length.html."))
class UDEPRECATED_PCGExPruneEdgesByLengthSettings : public UPCGExEdgesProcessorSettings
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
	//~End UPCGExEdgesProcessorSettings interface
};

struct FPCGExPruneEdgesByLengthContext final : FPCGExEdgesProcessorContext
{
	friend class UDEPRECATED_PCGExPruneEdgesByLengthSettings;
	friend class FPCGExPruneEdgesByLengthElement;
};

class FPCGExPruneEdgesByLengthElement final : public FPCGExEdgesProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(PruneEdgesByLength)

	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
