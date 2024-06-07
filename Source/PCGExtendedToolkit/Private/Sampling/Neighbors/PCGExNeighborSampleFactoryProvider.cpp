// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/Neighbors/PCGExNeighborSampleFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExCreateNeighborSample"
#define PCGEX_NAMESPACE PCGExCreateNeighborSample


bool UPCGExNeighborSampleOperation::PrepareForCluster(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster)
{
	Cluster = InCluster;

	if (PointState)
	{
		PointState->CaptureCluster(Context, Cluster);
		PointState->PrepareForTesting(Cluster->PointsIO);
	}

	if (ValueState)
	{
		ValueState->CaptureCluster(Context, Cluster);
		ValueState->PrepareForTesting(Cluster->PointsIO);
	}

	return
		(PointState && PointState->RequiresPerPointPreparation()) ||
		(ValueState && ValueState->RequiresPerPointPreparation());
}

bool UPCGExNeighborSampleOperation::IsOperationValid() { return bIsValidOperation; }

PCGExData::FPointIO& UPCGExNeighborSampleOperation::GetSourceIO() const
{
	return BaseSettings.NeighborSource == EPCGExGraphValueSource::Point ? *Cluster->PointsIO : *Cluster->EdgesIO;
}

void UPCGExNeighborSampleOperation::ProcessNodeForPoints(const int32 InNodeIndex) const
{
	PCGExCluster::FNode& TargetNode = Cluster->Nodes[InNodeIndex];

	if (PointState && !PointState->Test(TargetNode.PointIndex)) { return; }

	int32 CurrentDepth = 0;
	int32 Count = 0;
	double TotalWeight = 0;

	TArray<int32>* A = new TArray<int32>();
	TArray<int32>* B = new TArray<int32>();

	TArray<int32>* CurrentNeighbors = A;
	TArray<int32>* NextNeighbors = B;
	TSet<int32> VisitedNodes;

	VisitedNodes.Add(InNodeIndex);
	Cluster->GetConnectedNodes(InNodeIndex, *CurrentNeighbors, 1, VisitedNodes);

	PrepareNode(TargetNode);

	while (CurrentDepth <= BaseSettings.MaxDepth)
	{
		CurrentDepth++;

		if (CurrentDepth != BaseSettings.MaxDepth)
		{
			NextNeighbors->Empty();

			if (ValueState)
			{
				for (int i = 0; i < CurrentNeighbors->Num(); i++)
				{
					const int32 NIndex = (*CurrentNeighbors)[i];
					const int32 PtIndex = Cluster->Nodes[NIndex].PointIndex;
					if (ValueState->Test(PtIndex))
					{
						VisitedNodes.Add(NIndex);
						CurrentNeighbors->RemoveAt(i);
						i--;
					}
				}
			}

			for (const int32 Index : *CurrentNeighbors)
			{
				VisitedNodes.Add(Index);
				const PCGExCluster::FNode& NextNode = Cluster->Nodes[Index];
				double LocalWeight = 1;

				if (BaseSettings.BlendOver == EPCGExBlendOver::Distance)
				{
					const double Dist = FVector::Distance(TargetNode.Position, NextNode.Position);
					if (Dist > BaseSettings.MaxDistance) { continue; }
					LocalWeight = 1 - (Dist / BaseSettings.MaxDistance);
				}
				else
				{
					LocalWeight = BaseSettings.BlendOver == EPCGExBlendOver::Distance ? 1 - (CurrentDepth / (BaseSettings.MaxDepth - 1)) : BaseSettings.FixedBlend;
				}

				LocalWeight = SampleCurve(LocalWeight);

				BlendNodePoint(TargetNode, NextNode, LocalWeight);

				Count++;
				TotalWeight += LocalWeight;
			}

			for (const int32 Index : *CurrentNeighbors)
			{
				VisitedNodes.Add(Index);
				Cluster->GetConnectedNodes(Index, *NextNeighbors, 1, VisitedNodes);
			}

			std::swap(CurrentNeighbors, NextNeighbors);
		}
		else
		{
			for (const int32 Index : *CurrentNeighbors)
			{
				const PCGExCluster::FNode& NextNode = Cluster->Nodes[Index];
				double LocalWeight = 1;

				if (ValueState && !ValueState->Test(NextNode.PointIndex)) { continue; }

				if (BaseSettings.BlendOver == EPCGExBlendOver::Distance)
				{
					const double Dist = FVector::Distance(TargetNode.Position, NextNode.Position);
					if (Dist > BaseSettings.MaxDistance) { continue; }
					LocalWeight = 1 - (Dist / BaseSettings.MaxDistance);
				}
				else
				{
					LocalWeight = BaseSettings.BlendOver == EPCGExBlendOver::Distance ? 1 - (CurrentDepth / (BaseSettings.MaxDepth - 1)) : BaseSettings.FixedBlend;
				}

				LocalWeight = SampleCurve(LocalWeight);

				BlendNodePoint(TargetNode, NextNode, LocalWeight);

				Count++;
				TotalWeight += LocalWeight;
			}
		}
	}

	FinalizeNode(TargetNode, Count, TotalWeight);

	PCGEX_DELETE(A)
	PCGEX_DELETE(B)
}

void UPCGExNeighborSampleOperation::ProcessNodeForEdges(const int32 InNodeIndex) const
{
	if (PointState && !PointState->Test(InNodeIndex)) { return; }

	PCGExCluster::FNode& TargetNode = Cluster->Nodes[InNodeIndex];

	int32 CurrentDepth = 0;
	int32 Count = 0;
	double TotalWeight = 0;

	TArray<int32>* A = new TArray<int32>();
	TArray<int32>* B = new TArray<int32>();
	TArray<int32>* EA = new TArray<int32>();
	TArray<int32>* EB = new TArray<int32>();

	TArray<int32>* CurrentNeighbors = A;
	TArray<int32>* NextNeighbors = B;

	TArray<int32>* CurrentEdges = EA;
	TArray<int32>* NextEdges = EB;

	TSet<int32> VisitedNodes;
	TSet<int32> VisitedEdges;

	VisitedNodes.Add(InNodeIndex);
	Cluster->GetConnectedEdges(InNodeIndex, *CurrentNeighbors, *CurrentEdges, 1, VisitedNodes, VisitedEdges);

	PrepareNode(TargetNode);

	while (CurrentDepth <= BaseSettings.MaxDepth)
	{
		CurrentDepth++;

		if (CurrentDepth != BaseSettings.MaxDepth)
		{
			NextEdges->Empty();

			if (ValueState)
			{
				TSet<int32> IgnoredEndPoints;

				for (int i = 0; i < CurrentNeighbors->Num(); i++)
				{
					const int32 NIndex = (*CurrentNeighbors)[i];
					const int32 PtIndex = Cluster->Nodes[NIndex].PointIndex;
					if (ValueState->Test(PtIndex))
					{
						IgnoredEndPoints.Add(NIndex);
						VisitedNodes.Add(NIndex);
						CurrentNeighbors->RemoveAt(i);
						i--;
					}
				}

				for (int i = 0; i < CurrentEdges->Num(); i++)
				{
					const int32 EIndex = (*CurrentEdges)[i];
					const PCGExGraph::FIndexedEdge& Edge = Cluster->Edges[EIndex];
					if (IgnoredEndPoints.Contains(Edge.Start) || IgnoredEndPoints.Contains(Edge.End))
					{
						VisitedEdges.Add(EIndex);
						CurrentEdges->RemoveAt(i);
						i--;
					}
				}

				IgnoredEndPoints.Empty();
			}

			for (const int32 EdgeIndex : (*CurrentEdges))
			{
				VisitedEdges.Add(EdgeIndex);
				double LocalWeight = 1;

				if (BaseSettings.BlendOver == EPCGExBlendOver::Distance)
				{
					const double Dist = FVector::Distance(TargetNode.Position, Cluster->EdgesIO->GetInPoint(EdgeIndex).Transform.GetLocation());
					if (Dist > BaseSettings.MaxDistance) { continue; }
					LocalWeight = 1 - (Dist / BaseSettings.MaxDistance);
				}
				else
				{
					LocalWeight = BaseSettings.BlendOver == EPCGExBlendOver::Distance ? 1 - (CurrentDepth / (BaseSettings.MaxDepth - 1)) : BaseSettings.FixedBlend;
				}

				LocalWeight = SampleCurve(LocalWeight);

				BlendNodeEdge(TargetNode, EdgeIndex, LocalWeight);

				Count++;
				TotalWeight += LocalWeight;
			}

			for (const int32 Index : *CurrentNeighbors) { VisitedNodes.Add(Index); }
			for (const int32 Index : *CurrentNeighbors)
			{
				Cluster->GetConnectedEdges(Index, *NextNeighbors, *NextEdges, 1, VisitedNodes, VisitedEdges);
			}

			std::swap(CurrentNeighbors, NextNeighbors);
			std::swap(CurrentEdges, NextEdges);
		}
		else
		{
			for (const int32 EdgeIndex : (*CurrentEdges))
			{
				if (ValueState)
				{
					const PCGExGraph::FIndexedEdge& Edge = Cluster->Edges[EdgeIndex];
					if (!ValueState->Test(Edge.Start) || !ValueState->Test(Edge.End)) { continue; }
				}

				double LocalWeight = 1;

				if (BaseSettings.BlendOver == EPCGExBlendOver::Distance)
				{
					const double Dist = FVector::Distance(TargetNode.Position, Cluster->EdgesIO->GetInPoint(EdgeIndex).Transform.GetLocation());
					if (Dist > BaseSettings.MaxDistance) { continue; }
					LocalWeight = 1 - (Dist / BaseSettings.MaxDistance);
				}
				else
				{
					LocalWeight = BaseSettings.BlendOver == EPCGExBlendOver::Distance ? 1 - (CurrentDepth / (BaseSettings.MaxDepth - 1)) : BaseSettings.FixedBlend;
				}

				LocalWeight = SampleCurve(LocalWeight);

				BlendNodeEdge(TargetNode, EdgeIndex, LocalWeight);

				Count++;
				TotalWeight += LocalWeight;
			}
		}
	}

	FinalizeNode(TargetNode, Count, TotalWeight);
}

void UPCGExNeighborSampleOperation::PrepareNode(PCGExCluster::FNode& TargetNode) const
{
}

void UPCGExNeighborSampleOperation::BlendNodePoint(PCGExCluster::FNode& TargetNode, const PCGExCluster::FNode& OtherNode, const double Weight) const
{
}

void UPCGExNeighborSampleOperation::BlendNodeEdge(PCGExCluster::FNode& TargetNode, const int32 InEdgeIndex, const double Weight) const
{
}

void UPCGExNeighborSampleOperation::FinalizeNode(PCGExCluster::FNode& TargetNode, const int32 Count, const double TotalWeight) const
{
}

void UPCGExNeighborSampleOperation::FinalizeOperation()
{
}

void UPCGExNeighborSampleOperation::Cleanup()
{
	Super::Cleanup();
}

double UPCGExNeighborSampleOperation::SampleCurve(const double InTime) const { return WeightCurveObj->GetFloatValue(InTime); }

#if WITH_EDITOR
FString UPCGExNeighborSampleProviderSettings::GetDisplayName() const
{
	return TEXT("");
}
#endif

PCGExFactories::EType UPCGNeighborSamplerFactoryBase::GetFactoryType() const
{
	return PCGExFactories::EType::Sampler;
}

UPCGExNeighborSampleOperation* UPCGNeighborSamplerFactoryBase::CreateOperation() const
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
	UPCGNeighborSamplerFactoryBase* SamplerFactory = Cast<UPCGNeighborSamplerFactoryBase>(InFactory);
	SamplerFactory->Priority = Priority;
	SamplerFactory->SamplingSettings = SamplingSettings;
	GetInputFactories(InContext, PCGEx::SourcePointFilters, SamplerFactory->FilterFactories, PCGExFactories::ClusterFilters, false);

	TArray<UPCGExFilterFactoryBase*> FilterFactories;
	if (GetInputFactories(InContext, PCGEx::SourceUseValueIfFilters, FilterFactories, PCGExFactories::ClusterFilters, false))
	{
		SamplerFactory->ValueStateFactory = NewObject<UPCGExNodeStateFactory>();
		SamplerFactory->ValueStateFactory->FilterFactories.Append(FilterFactories);
	}

	return InFactory;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
