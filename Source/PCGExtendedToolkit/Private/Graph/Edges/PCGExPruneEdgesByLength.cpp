// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExPruneEdgesByLength.h"


#include "Sampling/PCGExSampling.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

FPCGExPruneEdgesByLengthContext::~FPCGExPruneEdgesByLengthContext()
{
	PCGEX_TERMINATE_ASYNC
}

PCGEX_INITIALIZE_CONTEXT(PruneEdgesByLength)
FPCGElementPtr UDEPRECATED_PCGExPruneEdgesByLengthSettings::CreateElement() const { return MakeShared<FPCGExPruneEdgesByLengthElement>(); }

bool FPCGExPruneEdgesByLengthElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPruneEdgesByLengthElement::Execute);

	PCGEX_CONTEXT(PruneEdgesByLength)

	PCGE_LOG(Error, GraphAndLog, FTEXT("Prune by Length has been deprecated and should be replaced. See https://nebukam.github.io/PCGExtendedToolkit/doc-clusters/clusters-prune-by-length.html."));

	return true;
}

#undef LOCTEXT_NAMESPACE
