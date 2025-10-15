﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/Neighbors/PCGExNeighborSampleFactoryProvider.h"

#include "PCGPin.h"


#define LOCTEXT_NAMESPACE "PCGExCreateNeighborSample"
#define PCGEX_NAMESPACE PCGExCreateNeighborSample

PCG_DEFINE_TYPE_INFO(FPCGExDataTypeInfoNeighborSampler, UPCGExNeighborSamplerFactoryData)

void FPCGExNeighborSampleOperation::PrepareForCluster(FPCGExContext* InContext, TSharedRef<PCGExCluster::FCluster> InCluster, TSharedRef<PCGExData::FFacade> InVtxDataFacade, TSharedRef<PCGExData::FFacade> InEdgeDataFacade)
{
	Cluster = InCluster;

	VtxDataFacade = InVtxDataFacade;
	EdgeDataFacade = InEdgeDataFacade;

	if (!VtxFilterFactories.IsEmpty())
	{
		PointFilters = MakeShared<PCGExClusterFilter::FManager>(InCluster, InVtxDataFacade, InEdgeDataFacade);
		PointFilters->SetSupportedTypes(&PCGExFactories::ClusterNodeFilters);
		PointFilters->Init(InContext, VtxFilterFactories);
	}

	if (!ValueFilterFactories.IsEmpty())
	{
		ValueFilters = MakeShared<PCGExClusterFilter::FManager>(InCluster, InVtxDataFacade, InEdgeDataFacade);
		ValueFilters->SetSupportedTypes(&PCGExFactories::ClusterNodeFilters);
		ValueFilters->Init(InContext, ValueFilterFactories);
	}
}

bool FPCGExNeighborSampleOperation::IsOperationValid() { return bIsValidOperation; }

TSharedRef<PCGExData::FPointIO> FPCGExNeighborSampleOperation::GetSourceIO() const { return GetSourceDataFacade()->Source; }

TSharedRef<PCGExData::FFacade> FPCGExNeighborSampleOperation::GetSourceDataFacade() const
{
	return SamplingConfig.NeighborSource == EPCGExClusterElement::Vtx ? VtxDataFacade.ToSharedRef() : EdgeDataFacade.ToSharedRef();
}

void FPCGExNeighborSampleOperation::PrepareForLoops(const TArray<PCGExMT::FScope>& Loops)
{
	// Do pre-allocation here
}

void FPCGExNeighborSampleOperation::ProcessNode(const int32 NodeIndex, const PCGExMT::FScope& Scope)
{
	const PCGExCluster::FNode& Node = (*Cluster->Nodes)[NodeIndex];

	if (PointFilters && !PointFilters->Test(Node)) { return; }

	int32 CurrentDepth = 0;
	int32 Count = 0;
	double TotalWeight = 0;

	const TUniquePtr<TArray<PCGExGraph::FLink>> A = MakeUnique<TArray<PCGExGraph::FLink>>();
	const TUniquePtr<TArray<PCGExGraph::FLink>> B = MakeUnique<TArray<PCGExGraph::FLink>>();

	TArray<PCGExGraph::FLink>* CurrentNeighbors = A.Get();
	TArray<PCGExGraph::FLink>* NextNeighbors = B.Get();
	TSet<int32> VisitedNodes;

	VisitedNodes.Add(NodeIndex);
	CurrentNeighbors->Append(Node.Links);

	PrepareNode(Node, Scope);
	const FVector Origin = Cluster->GetPos(Node);

	const int32 SafeMaxDepth = FMath::Min(1, SamplingConfig.MaxDepth);

	while (CurrentDepth <= SafeMaxDepth)
	{
		if (CurrentNeighbors->IsEmpty()) { break; }
		CurrentDepth++;

		for (const PCGExGraph::FLink Lk : (*CurrentNeighbors))
		{
			VisitedNodes.Add(Lk.Node);
			double LocalWeight;

			if (SamplingConfig.BlendOver == EPCGExBlendOver::Distance)
			{
				const double Dist = FVector::Dist(Origin, Cluster->GetPos(Lk)); // Use Neighbor.FromNode to accumulate per-path distance 
				if (Dist > SamplingConfig.MaxDistance) { continue; }
				LocalWeight = 1 - (Dist / SamplingConfig.MaxDistance);
			}
			else
			{
				LocalWeight = SamplingConfig.BlendOver == EPCGExBlendOver::Index ? 1 - (CurrentDepth / SafeMaxDepth) : SamplingConfig.FixedBlend;
			}

			LocalWeight = WeightCurveObj->Eval(LocalWeight);

			if (SamplingConfig.NeighborSource == EPCGExClusterElement::Vtx) { SampleNeighborNode(Node, Lk, LocalWeight, Scope); }
			else { SampleNeighborEdge(Node, Lk, LocalWeight, Scope); }

			Count++;
			TotalWeight += LocalWeight;
		}

		if (CurrentDepth >= SamplingConfig.MaxDepth) { break; }

		// Gather next depth

		NextNeighbors->Reset();
		for (const PCGExGraph::FLink& Old : (*CurrentNeighbors))
		{
			const PCGExGraph::NodeLinks& Neighbors = Cluster->GetNode(Old.Node)->Links;
			if (ValueFilters)
			{
				for (const PCGExGraph::FLink Next : Neighbors)
				{
					int32 NextIndex = Next.Node;
					if (VisitedNodes.Contains(NextIndex)) { continue; }
					if (!ValueFilters->Results[Cluster->GetNode(Next)->PointIndex])
					{
						VisitedNodes.Add(NextIndex);
						continue;
					}
					NextNeighbors->Add(Next);
				}
			}
			else
			{
				for (const PCGExGraph::FLink Next : Neighbors)
				{
					if (VisitedNodes.Contains(Next.Node)) { continue; }
					NextNeighbors->Add(Next);
				}
			}
		}

		std::swap(CurrentNeighbors, NextNeighbors);
	}

	FinalizeNode(Node, Count, TotalWeight, Scope);
}

void FPCGExNeighborSampleOperation::PrepareNode(const PCGExCluster::FNode& TargetNode, const PCGExMT::FScope& Scope) const
{
}

void FPCGExNeighborSampleOperation::SampleNeighborNode(const PCGExCluster::FNode& TargetNode, const PCGExGraph::FLink Lk, const double Weight, const PCGExMT::FScope& Scope)
{
}

void FPCGExNeighborSampleOperation::SampleNeighborEdge(const PCGExCluster::FNode& TargetNode, const PCGExGraph::FLink Lk, const double Weight, const PCGExMT::FScope& Scope)
{
}

void FPCGExNeighborSampleOperation::FinalizeNode(const PCGExCluster::FNode& TargetNode, const int32 Count, const double TotalWeight, const PCGExMT::FScope& Scope)
{
}

void FPCGExNeighborSampleOperation::CompleteOperation()
{
}

#if WITH_EDITOR
FString UPCGExNeighborSampleProviderSettings::GetDisplayName() const
{
	return TEXT("");
}
#endif

TSharedPtr<FPCGExNeighborSampleOperation> UPCGExNeighborSamplerFactoryData::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(NeighborSampleOperation)
	PCGEX_SAMPLER_CREATE_OPERATION
	return NewOperation;
}

void UPCGExNeighborSamplerFactoryData::RegisterVtxBuffersDependencies(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, PCGExData::FFacadePreloader& FacadePreloader) const
{
	if (!VtxFilterFactories.IsEmpty())
	{
		for (const TObjectPtr<const UPCGExPointFilterFactoryData>& Filter : VtxFilterFactories) { Filter->RegisterBuffersDependencies(InContext, FacadePreloader); }
	}

	if (!ValueFilterFactories.IsEmpty())
	{
		for (const TObjectPtr<const UPCGExPointFilterFactoryData>& Filter : ValueFilterFactories) { Filter->RegisterBuffersDependencies(InContext, FacadePreloader); }
	}
}

void UPCGExNeighborSamplerFactoryData::RegisterAssetDependencies(FPCGExContext* InContext) const
{
	Super::RegisterAssetDependencies(InContext);
	InContext->AddAssetDependency(SamplingConfig.WeightCurve.ToSoftObjectPath());
}

TArray<FPCGPinProperties> UPCGExNeighborSampleProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	bool bIsRequired = false;
	if (SupportsVtxFilters(bIsRequired))
	{
		if (bIsRequired) { PCGEX_PIN_FILTERS(PCGExPointFilter::SourceVtxFiltersLabel, "Filters applied to vtx", Required) }
		else { PCGEX_PIN_FILTERS(PCGExPointFilter::SourceVtxFiltersLabel, "Filters applied to vtx", Advanced) }
	}
	if (SupportsEdgeFilters(bIsRequired))
	{
		if (bIsRequired) { PCGEX_PIN_FILTERS(PCGExPointFilter::SourceEdgeFiltersLabel, "Filters applied to edges", Required) }
		else { PCGEX_PIN_FILTERS(PCGExPointFilter::SourceEdgeFiltersLabel, "Filters applied to edges", Advanced) }
	}
	PCGEX_PIN_FILTERS(PCGEx::SourceUseValueIfFilters, "Filters used to check if a node can be used as a value source or not.", Advanced)
	return PinProperties;
}

bool UPCGExNeighborSampleProviderSettings::SupportsVtxFilters(bool& bIsRequired) const
{
	bIsRequired = false;
	return true;
}

bool UPCGExNeighborSampleProviderSettings::SupportsEdgeFilters(bool& bIsRequired) const
{
	bIsRequired = false;
	return false;
}

UPCGExFactoryData* UPCGExNeighborSampleProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExNeighborSamplerFactoryData* SamplerFactory = Cast<UPCGExNeighborSamplerFactoryData>(InFactory);

	SamplerFactory->Priority = Priority;
	SamplerFactory->SamplingConfig = SamplingConfig;

	GetInputFactories(
		InContext, PCGExPointFilter::SourceVtxFiltersLabel, SamplerFactory->VtxFilterFactories,
		PCGExFactories::ClusterNodeFilters, false);

	GetInputFactories(
		InContext, PCGExPointFilter::SourceVtxFiltersLabel, SamplerFactory->EdgesFilterFactories,
		PCGExFactories::ClusterEdgeFilters, false);

	GetInputFactories(
		InContext, PCGEx::SourceUseValueIfFilters, SamplerFactory->ValueFilterFactories,
		PCGExFactories::ClusterNodeFilters, false);

	return Super::CreateFactory(InContext, SamplerFactory);
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
