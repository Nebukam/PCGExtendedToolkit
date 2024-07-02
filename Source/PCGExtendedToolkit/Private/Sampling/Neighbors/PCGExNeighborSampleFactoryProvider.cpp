// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/Neighbors/PCGExNeighborSampleFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExCreateNeighborSample"
#define PCGEX_NAMESPACE PCGExCreateNeighborSample


void UPCGExNeighborSampleOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	const UPCGExNeighborSampleOperation* TypedOther = Cast<UPCGExNeighborSampleOperation>(Other);
	if (TypedOther)
	{
		BaseSettings = TypedOther->BaseSettings;
		WeightCurveObj = TypedOther->WeightCurveObj;
	}
}

void UPCGExNeighborSampleOperation::PrepareForCluster(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster, PCGExData::FFacade* InVtxDataFacade, PCGExData::FFacade* InEdgeDataFacade)
{
	Cluster = InCluster;

	VtxDataFacade = InVtxDataFacade;
	EdgeDataFacade = InEdgeDataFacade;

	if (!PointFilterFactories.IsEmpty())
	{
		PointFilters = new PCGExClusterFilter::TManager(InCluster, InVtxDataFacade, InEdgeDataFacade);
		PointFilters->Init(InContext, PointFilterFactories);
	}

	if (!ValueFilterFactories.IsEmpty())
	{
		ValueFilters = new PCGExClusterFilter::TManager(InCluster, InVtxDataFacade, InEdgeDataFacade);
		ValueFilters->Init(InContext, ValueFilterFactories);
	}
}

bool UPCGExNeighborSampleOperation::IsOperationValid() { return bIsValidOperation; }

PCGExData::FPointIO* UPCGExNeighborSampleOperation::GetSourceIO() const { return GetSourceDataFacade()->Source; }

PCGExData::FFacade* UPCGExNeighborSampleOperation::GetSourceDataFacade() const
{
	return BaseSettings.NeighborSource == EPCGExGraphValueSource::Vtx ? VtxDataFacade : EdgeDataFacade;
}

void UPCGExNeighborSampleOperation::ProcessNode(const int32 NodeIndex) const
{
	const PCGExCluster::FNode& Node = (*Cluster->Nodes)[NodeIndex];

	if (PointFilters && !PointFilters->Test(Node)) { return; }

	int32 CurrentDepth = 0;
	int32 Count = 0;
	double TotalWeight = 0;

	TArray<PCGExCluster::FExpandedNeighbor>* A = new TArray<PCGExCluster::FExpandedNeighbor>();
	TArray<PCGExCluster::FExpandedNeighbor>* B = new TArray<PCGExCluster::FExpandedNeighbor>();

	TArray<PCGExCluster::FExpandedNeighbor>* CurrentNeighbors = A;
	TArray<PCGExCluster::FExpandedNeighbor>* NextNeighbors = B;
	TSet<int32> VisitedNodes;

	const TArray<PCGExCluster::FExpandedNode*>& ExpandedNodesRef = (*Cluster->ExpandedNodes);

	VisitedNodes.Add(NodeIndex);
	CurrentNeighbors->Append(ExpandedNodesRef[NodeIndex]->Neighbors);

	PrepareNode(Node);


	while (CurrentDepth <= BaseSettings.MaxDepth)
	{
		if (NextNeighbors->IsEmpty()) { break; }
		CurrentDepth++;

		for (const PCGExCluster::FExpandedNeighbor& Neighbor : (*CurrentNeighbors))
		{
			VisitedNodes.Add(Neighbor.Node->NodeIndex);
			double LocalWeight;

			if (BaseSettings.BlendOver == EPCGExBlendOver::Distance)
			{
				const double Dist = FVector::Dist(Node.Position, Neighbor.Node->Position); // Use Neighbor.FromNode to accumulate per-path distance 
				if (Dist > BaseSettings.MaxDistance) { continue; }
				LocalWeight = 1 - (Dist / BaseSettings.MaxDistance);
			}
			else
			{
				LocalWeight = BaseSettings.BlendOver == EPCGExBlendOver::Distance ? 1 - (CurrentDepth / (BaseSettings.MaxDepth - 1)) : BaseSettings.FixedBlend;
			}

			LocalWeight = SampleCurve(LocalWeight);

			if (BaseSettings.NeighborSource == EPCGExGraphValueSource::Vtx) { BlendNodePoint(Node, Neighbor, LocalWeight); }
			else { BlendNodeEdge(Node, Neighbor, LocalWeight); }

			Count++;
			TotalWeight += LocalWeight;
		}

		if (CurrentDepth >= BaseSettings.MaxDepth) { break; }

		// Gather next depth

		NextNeighbors->Reset();
		for (const PCGExCluster::FExpandedNeighbor& Old : (*CurrentNeighbors))
		{
			const TArray<PCGExCluster::FExpandedNeighbor>& Neighbors = ExpandedNodesRef[Old.Node->NodeIndex]->Neighbors;
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

	PCGEX_DELETE(A)
	PCGEX_DELETE(B)
}

void UPCGExNeighborSampleOperation::PrepareNode(const PCGExCluster::FNode& TargetNode) const
{
}

void UPCGExNeighborSampleOperation::BlendNodePoint(const PCGExCluster::FNode& TargetNode, const PCGExCluster::FExpandedNeighbor& Neighbor, const double Weight) const
{
}

void UPCGExNeighborSampleOperation::BlendNodeEdge(const PCGExCluster::FNode& TargetNode, const PCGExCluster::FExpandedNeighbor& Neighbor, const double Weight) const
{
}

void UPCGExNeighborSampleOperation::FinalizeNode(const PCGExCluster::FNode& TargetNode, const int32 Count, const double TotalWeight) const
{
}

void UPCGExNeighborSampleOperation::FinalizeOperation()
{
}

void UPCGExNeighborSampleOperation::Cleanup()
{
	Super::Cleanup();
	PCGEX_DELETE(PointFilters)
	PCGEX_DELETE(ValueFilters)
}

double UPCGExNeighborSampleOperation::SampleCurve(const double InTime) const { return WeightCurveObj->GetFloatValue(InTime); }

#if WITH_EDITOR
FString UPCGExNeighborSampleProviderSettings::GetDisplayName() const
{
	return TEXT("");
}
#endif

PCGExFactories::EType UPCGExNeighborSamplerFactoryBase::GetFactoryType() const
{
	return PCGExFactories::EType::Sampler;
}

UPCGExNeighborSampleOperation* UPCGExNeighborSamplerFactoryBase::CreateOperation() const
{
	UPCGExNeighborSampleOperation* NewOperation = NewObject<UPCGExNeighborSampleOperation>();

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

FName UPCGExNeighborSampleProviderSettings::GetMainOutputLabel() const { return PCGExNeighborSample::OutputSamplerLabel; }

UPCGExParamFactoryBase* UPCGExNeighborSampleProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExNeighborSamplerFactoryBase* SamplerFactory = Cast<UPCGExNeighborSamplerFactoryBase>(InFactory);

	SamplerFactory->Priority = Priority;
	SamplerFactory->SamplingSettings = SamplingSettings;

	GetInputFactories(InContext, PCGEx::SourcePointFilters, SamplerFactory->PointFilterFactories, PCGExFactories::ClusterNodeFilters, false);
	GetInputFactories(InContext, PCGEx::SourceUseValueIfFilters, SamplerFactory->ValueFilterFactories, PCGExFactories::ClusterNodeFilters, false);

	return InFactory;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
