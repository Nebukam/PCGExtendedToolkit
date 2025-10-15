﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/FloodFill/FillControls/PCGExFillControlEdgeFilters.h"


#include "Graph/Filters/PCGExClusterFilter.h"
#include "Graph/FloodFill/FillControls/PCGExFillControlsFactoryProvider.h"

bool FPCGExFillControlEdgeFilters::PrepareForDiffusions(FPCGExContext* InContext, const TSharedPtr<PCGExFloodFill::FFillControlsHandler>& InHandler)
{
	if (!FPCGExFillControlOperation::PrepareForDiffusions(InContext, InHandler)) { return false; }

	const UPCGExFillControlsFactoryEdgeFilters* TypedFactory = Cast<UPCGExFillControlsFactoryEdgeFilters>(Factory);

	EdgeFilterManager = MakeShared<PCGExClusterFilter::FManager>(Cluster.ToSharedRef(), InHandler->VtxDataFacade.ToSharedRef(), InHandler->EdgeDataFacade.ToSharedRef());
	EdgeFilterManager->SetSupportedTypes(&PCGExFactories::ClusterEdgeFilters);
	EdgeFilterManager->bUseEdgeAsPrimary = true;

	return EdgeFilterManager->Init(InContext, TypedFactory->FilterFactories);
}

bool FPCGExFillControlEdgeFilters::IsValidCapture(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate)
{
	const PCGExGraph::FEdge* E = Cluster->GetEdge(Candidate.Link);
	// Orient edge in diffusion direction
	const PCGExGraph::FEdge Edge(E->Index, Candidate.Link.Node, Candidate.Node->PointIndex, E->PointIndex, E->IOIndex);
	return EdgeFilterManager->Test(Edge);
}

bool FPCGExFillControlEdgeFilters::IsValidProbe(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate)
{
	if (Candidate.Link.Edge == -1) { return true; }
	const PCGExGraph::FEdge* E = Cluster->GetEdge(Candidate.Link);
	// Orient edge in diffusion direction
	const PCGExGraph::FEdge Edge(E->Index, Candidate.Link.Node, Candidate.Node->PointIndex, E->PointIndex, E->IOIndex);
	return EdgeFilterManager->Test(Edge);
}

bool FPCGExFillControlEdgeFilters::IsValidCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& From, const PCGExFloodFill::FCandidate& Candidate)
{
	const PCGExGraph::FEdge* E = Cluster->GetEdge(Candidate.Link);
	// Orient edge in diffusion direction
	const PCGExGraph::FEdge Edge(E->Index, Candidate.Link.Node, Candidate.Node->PointIndex, E->PointIndex, E->IOIndex);
	return EdgeFilterManager->Test(Edge);
}

TSharedPtr<FPCGExFillControlOperation> UPCGExFillControlsFactoryEdgeFilters::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(FillControlEdgeFilters)
	PCGEX_FORWARD_FILLCONTROL_OPERATION
	return NewOperation;
}

TArray<FPCGPinProperties> UPCGExFillControlsEdgeFiltersProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_FILTERS(PCGExPointFilter::SourceEdgeFiltersLabel, TEXT("Filters used on edges."), Required)
	return PinProperties;
}

UPCGExFactoryData* UPCGExFillControlsEdgeFiltersProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExFillControlsFactoryEdgeFilters* NewFactory = InContext->ManagedObjects->New<UPCGExFillControlsFactoryEdgeFilters>();
	PCGEX_FORWARD_FILLCONTROL_FACTORY
	Super::CreateFactory(InContext, NewFactory);

	if (!GetInputFactories(
		InContext, PCGExPointFilter::SourceEdgeFiltersLabel, NewFactory->FilterFactories,
		PCGExFactories::ClusterEdgeFilters))
	{
		InContext->ManagedObjects->Destroy(NewFactory);
		return nullptr;
	}

	return NewFactory;
}

#if WITH_EDITOR
FString UPCGExFillControlsEdgeFiltersProviderSettings::GetDisplayName() const
{
	return GetDefaultNodeTitle().ToString().Replace(TEXT("PCGEx | Fill Control"), TEXT("FC"));
}
#endif
