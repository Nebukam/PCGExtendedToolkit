// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/Neighbors/PCGExNeighborSampleFactoryProvider.h"

#include "PCGPin.h"


#define LOCTEXT_NAMESPACE "PCGExCreateNeighborSample"
#define PCGEX_NAMESPACE PCGExCreateNeighborSample


void UPCGExNeighborSampleOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExNeighborSampleOperation* TypedOther = Cast<UPCGExNeighborSampleOperation>(Other))
	{
		SamplingConfig = TypedOther->SamplingConfig;
		WeightCurveObj = TypedOther->WeightCurveObj;
	}
}

void UPCGExNeighborSampleOperation::PrepareForCluster(FPCGExContext* InContext, TSharedRef<PCGExCluster::FCluster> InCluster, TSharedRef<PCGExData::FFacade> InVtxDataFacade, TSharedRef<PCGExData::FFacade> InEdgeDataFacade)
{
	Cluster = InCluster;

	VtxDataFacade = InVtxDataFacade;
	EdgeDataFacade = InEdgeDataFacade;

	if (!PointFilterFactories.IsEmpty())
	{
		PointFilters = MakeShared<PCGExClusterFilter::FManager>(InCluster, InVtxDataFacade, InEdgeDataFacade);
		PointFilters->Init(InContext, PointFilterFactories);
	}

	if (!ValueFilterFactories.IsEmpty())
	{
		ValueFilters = MakeShared<PCGExClusterFilter::FManager>(InCluster, InVtxDataFacade, InEdgeDataFacade);
		ValueFilters->Init(InContext, ValueFilterFactories);
	}
}

bool UPCGExNeighborSampleOperation::IsOperationValid() { return bIsValidOperation; }

TSharedRef<PCGExData::FPointIO> UPCGExNeighborSampleOperation::GetSourceIO() const { return GetSourceDataFacade()->Source; }

TSharedRef<PCGExData::FFacade> UPCGExNeighborSampleOperation::GetSourceDataFacade() const
{
	return SamplingConfig.NeighborSource == EPCGExClusterComponentSource::Vtx ? VtxDataFacade.ToSharedRef() : EdgeDataFacade.ToSharedRef();
}

void UPCGExNeighborSampleOperation::ProcessNode(const int32 NodeIndex) const
{
	const PCGExCluster::FNode& Node = (*Cluster->Nodes)[NodeIndex];

	if (PointFilters && !PointFilters->Test(Node)) { return; }

	int32 CurrentDepth = 0;
	int32 Count = 0;
	double TotalWeight = 0;

	const TUniquePtr<TArray<PCGExCluster::FExpandedNeighbor>> A = MakeUnique<TArray<PCGExCluster::FExpandedNeighbor>>();
	const TUniquePtr<TArray<PCGExCluster::FExpandedNeighbor>> B = MakeUnique<TArray<PCGExCluster::FExpandedNeighbor>>();

	TArray<PCGExCluster::FExpandedNeighbor>* CurrentNeighbors = A.Get();
	TArray<PCGExCluster::FExpandedNeighbor>* NextNeighbors = B.Get();
	TSet<int32> VisitedNodes;

	const TArray<PCGExCluster::FExpandedNode>& ExpandedNodesRef = (*Cluster->ExpandedNodes);

	VisitedNodes.Add(NodeIndex);
	CurrentNeighbors->Append(ExpandedNodesRef[NodeIndex].Neighbors);

	PrepareNode(Node);
	const FVector Origin = Cluster->GetPos(Node);


	while (CurrentDepth <= SamplingConfig.MaxDepth)
	{
		if (CurrentNeighbors->IsEmpty()) { break; }
		CurrentDepth++;

		for (const PCGExCluster::FExpandedNeighbor& Neighbor : (*CurrentNeighbors))
		{
			VisitedNodes.Add(Neighbor.Node->NodeIndex);
			double LocalWeight;

			if (SamplingConfig.BlendOver == EPCGExBlendOver::Distance)
			{
				const double Dist = FVector::Dist(Origin, Cluster->GetPos(Neighbor)); // Use Neighbor.FromNode to accumulate per-path distance 
				if (Dist > SamplingConfig.MaxDistance) { continue; }
				LocalWeight = 1 - (Dist / SamplingConfig.MaxDistance);
			}
			else
			{
				LocalWeight = SamplingConfig.BlendOver == EPCGExBlendOver::Index ? 1 - (CurrentDepth / (SamplingConfig.MaxDepth)) : SamplingConfig.FixedBlend;
			}

			LocalWeight = SampleCurve(LocalWeight);

			if (SamplingConfig.NeighborSource == EPCGExClusterComponentSource::Vtx) { BlendNodePoint(Node, Neighbor, LocalWeight); }
			else { BlendNodeEdge(Node, Neighbor, LocalWeight); }

			Count++;
			TotalWeight += LocalWeight;
		}

		if (CurrentDepth >= SamplingConfig.MaxDepth) { break; }

		// Gather next depth

		NextNeighbors->Reset();
		for (const PCGExCluster::FExpandedNeighbor& Old : (*CurrentNeighbors))
		{
			const TArray<PCGExCluster::FExpandedNeighbor>& Neighbors = ExpandedNodesRef[Old.Node->NodeIndex].Neighbors;
			if (ValueFilters)
			{
				for (const PCGExCluster::FExpandedNeighbor& Next : Neighbors)
				{
					int32 NextIndex = Next.Node->NodeIndex;
					if (VisitedNodes.Contains(NextIndex)) { continue; }
					if (!ValueFilters->Results[Next.Node->PointIndex])
					{
						VisitedNodes.Add(NextIndex);
						continue;
					}
					NextNeighbors->Add(Next);
				}
			}
			else
			{
				for (const PCGExCluster::FExpandedNeighbor& Next : Neighbors)
				{
					if (VisitedNodes.Contains(Next.Node->NodeIndex)) { continue; }
					NextNeighbors->Add(Next);
				}
			}
		}

		std::swap(CurrentNeighbors, NextNeighbors);
	}

	FinalizeNode(Node, Count, TotalWeight);
}

void UPCGExNeighborSampleOperation::CompleteOperation()
{
}

void UPCGExNeighborSampleOperation::Cleanup()
{
	PointFilters.Reset();
	ValueFilters.Reset();
	VtxDataFacade.Reset();
	EdgeDataFacade.Reset();
	Super::Cleanup();
}

#if WITH_EDITOR
FString UPCGExNeighborSampleProviderSettings::GetDisplayName() const
{
	return TEXT("");
}
#endif

UPCGExNeighborSampleOperation* UPCGExNeighborSamplerFactoryBase::CreateOperation(FPCGExContext* InContext) const
{
	UPCGExNeighborSampleOperation* NewOperation = InContext->ManagedObjects->New<UPCGExNeighborSampleOperation>();
	PCGEX_SAMPLER_CREATE
	return NewOperation;
}

TArray<FPCGPinProperties> UPCGExNeighborSampleProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGEx::SourcePointFilters, "Filters used to check which node will be processed by the sampler or not.", Advanced, {})
	PCGEX_PIN_PARAMS(PCGEx::SourceUseValueIfFilters, "Filters used to check if a node can be used as a value source or not.", Advanced, {})
	return PinProperties;
}

UPCGExParamFactoryBase* UPCGExNeighborSampleProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExNeighborSamplerFactoryBase* SamplerFactory = Cast<UPCGExNeighborSamplerFactoryBase>(InFactory);

	SamplerFactory->Priority = Priority;
	SamplerFactory->SamplingConfig = SamplingConfig;

	GetInputFactories(
		InContext, PCGEx::SourcePointFilters, SamplerFactory->PointFilterFactories,
		PCGExFactories::ClusterNodeFilters, false);
	GetInputFactories(
		InContext, PCGEx::SourceUseValueIfFilters, SamplerFactory->ValueFilterFactories,
		PCGExFactories::ClusterNodeFilters, false);

	return InFactory;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
