﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/Nodes/PCGExNodeAdjacencyFilter.h"


#include "Data/PCGExDataPreloader.h"
#include "Details/PCGExDetailsSettings.h"
#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExNodeAdjacencyFilter"
#define PCGEX_NAMESPACE NodeAdjacencyFilter

PCGEX_SETTING_VALUE_IMPL(FPCGExNodeAdjacencyFilterConfig, OperandA, double, CompareAgainst, OperandA, OperandAConstant)
PCGEX_SETTING_VALUE_IMPL(FPCGExNodeAdjacencyFilterConfig, OperandB, double, EPCGExInputValueType::Attribute, OperandB, 0)

void UPCGExNodeAdjacencyFilterFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { FacadePreloader.Register<double>(InContext, Config.OperandA); }
	if (Config.OperandBSource == EPCGExClusterElement::Vtx) { FacadePreloader.Register<double>(InContext, Config.OperandB); }
}

bool UPCGExNodeAdjacencyFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_CONDITIONAL(Config.CompareAgainst == EPCGExInputValueType::Attribute, Config.OperandA, Consumable)
	PCGEX_CONSUMABLE_SELECTOR(Config.OperandB, Consumable)

	return true;
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExNodeAdjacencyFilterFactory::CreateFilter() const
{
	return MakeShared<FNodeAdjacencyFilter>(this);
}

bool FNodeAdjacencyFilter::Init(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
{
	if (!IFilter::Init(InContext, InCluster, InPointDataFacade, InEdgeDataFacade)) { return false; }

	bCaptureFromNodes = TypedFilterFactory->Config.OperandBSource != EPCGExClusterElement::Edge;

	OperandA = TypedFilterFactory->Config.GetValueSettingOperandA();
	if (!OperandA->Init(PointDataFacade, false)) { return false; }

	if (!Adjacency.Init(InContext, PointDataFacade.ToSharedRef())) { return false; }

	OperandB = TypedFilterFactory->Config.GetValueSettingOperandB();
	if (!OperandB->Init(bCaptureFromNodes ? PointDataFacade : EdgeDataFacade, false)) { return false; }

#define PCGEX_SUB_TEST_FUNC TestSubFunc = [&](const PCGExCluster::FNode& Node, const TArray<PCGExCluster::FNode>& NodesRef, const double A)

	if (Adjacency.bTestAllNeighbors)
	{
		// Each adjacent sample must pass the comparison, exit early.
		if (Adjacency.Consolidation == EPCGExAdjacencyGatherMode::Individual)
		{
			if (bCaptureFromNodes)
			{
				PCGEX_SUB_TEST_FUNC
				{
					double B = 0;
					for (const PCGExGraph::FLink Lk : Node.Links)
					{
						B = OperandB->Read(NodesRef[Lk.Node].PointIndex);
						if (!PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance)) { return false; }
					}
					return true;
				};
			}
			else
			{
				PCGEX_SUB_TEST_FUNC
				{
					double B = 0;
					for (const PCGExGraph::FLink Lk : Node.Links)
					{
						B = OperandB->Read(Lk.Edge);
						if (!PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance)) { return false; }
					}
					return true;
				};
			}
		}

		// First, build the consolidated operand B
		switch (Adjacency.Consolidation)
		{
		case EPCGExAdjacencyGatherMode::Average:
			if (bCaptureFromNodes)
			{
				PCGEX_SUB_TEST_FUNC
				{
					double B = 0;
					for (const PCGExGraph::FLink Lk : Node.Links) { B += OperandB->Read(NodesRef[Lk.Node].PointIndex); }					;
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B / FMath::Max(1, static_cast<double>(Node.Num())), TypedFilterFactory->Config.Tolerance);
				};
			}
			else
			{
				PCGEX_SUB_TEST_FUNC
				{
					double B = 0;
					for (const PCGExGraph::FLink Lk : Node.Links) { B += OperandB->Read(Lk.Edge); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B / FMath::Max(1, static_cast<double>(Node.Num())), TypedFilterFactory->Config.Tolerance);
				};
			}

			break;
		case EPCGExAdjacencyGatherMode::Min:
			if (bCaptureFromNodes)
			{
				PCGEX_SUB_TEST_FUNC
				{
					double B = MAX_dbl;
					for (const PCGExGraph::FLink Lk : Node.Links) { B = FMath::Min(B, OperandB->Read(NodesRef[Lk.Node].PointIndex)); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance);
				};
			}
			else
			{
				PCGEX_SUB_TEST_FUNC
				{
					double B = MAX_dbl;
					for (const PCGExGraph::FLink Lk : Node.Links) { B = FMath::Min(B, OperandB->Read(Lk.Edge)); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance);
				};
			}
			break;
		case EPCGExAdjacencyGatherMode::Max:
			if (bCaptureFromNodes)
			{
				PCGEX_SUB_TEST_FUNC
				{
					double B = MIN_dbl_neg;
					for (const PCGExGraph::FLink Lk : Node.Links) { B = FMath::Max(B, OperandB->Read(NodesRef[Lk.Node].PointIndex)); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance);
				};
			}
			else
			{
				PCGEX_SUB_TEST_FUNC
				{
					double B = MIN_dbl_neg;
					for (const PCGExGraph::FLink Lk : Node.Links) { B = FMath::Max(B, OperandB->Read(Lk.Edge)); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance);
				};
			}
			break;
		case EPCGExAdjacencyGatherMode::Sum:
			if (bCaptureFromNodes)
			{
				PCGEX_SUB_TEST_FUNC
				{
					double B = 0;
					for (const PCGExGraph::FLink Lk : Node.Links) { B += OperandB->Read(NodesRef[Lk.Node].PointIndex); }					;
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance);
				};
			}
			else
			{
				PCGEX_SUB_TEST_FUNC
				{
					double B = 0;
					for (const PCGExGraph::FLink Lk : Node.Links) { B += OperandB->Read(Lk.Edge); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance);
				};
			}
			break;
		default:
		case EPCGExAdjacencyGatherMode::Individual:
			break;
		}
	}

	if (TestSubFunc) { return true; }

	if (bCaptureFromNodes)
	{
		PCGEX_SUB_TEST_FUNC
		{
			// Only some adjacent samples must pass the comparison
			const int32 Threshold = Adjacency.GetThreshold(Node);
			if (Threshold == -1) { return false; }
			int32 LocalSuccessCount = 0;
			double B = 0;

			for (const PCGExGraph::FLink Lk : Node.Links)
			{
				B = OperandB->Read(NodesRef[Lk.Node].PointIndex);
				if (PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance)) { LocalSuccessCount++; }
			}

			return PCGExCompare::Compare(Adjacency.ThresholdComparison, LocalSuccessCount, Threshold, Adjacency.ThresholdTolerance);
		};
	}
	else
	{
		PCGEX_SUB_TEST_FUNC
		{
			// Only some adjacent samples must pass the comparison
			const int32 Threshold = Adjacency.GetThreshold(Node);
			if (Threshold == -1) { return false; }
			int32 LocalSuccessCount = 0;
			double B = 0;

			for (const PCGExGraph::FLink Lk : Node.Links)
			{
				B = OperandB->Read(Lk.Edge);
				if (PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance)) { LocalSuccessCount++; }
			}

			return PCGExCompare::Compare(Adjacency.ThresholdComparison, LocalSuccessCount, Threshold, Adjacency.ThresholdTolerance);
		};
	}

#undef PCGEX_SUB_TEST_FUNC

	return true;
}

bool FNodeAdjacencyFilter::Test(const PCGExCluster::FNode& Node) const
{
	return TestSubFunc(Node, *Cluster->Nodes, OperandA->Read(Node.PointIndex));
}

FNodeAdjacencyFilter::~FNodeAdjacencyFilter()
{
	TypedFilterFactory = nullptr;
}


PCGEX_CREATE_FILTER_FACTORY(NodeAdjacency)

#if WITH_EDITOR
FString UPCGExNodeAdjacencyFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = PCGEx::GetSelectorDisplayName(Config.OperandA) + PCGExCompare::ToString(Config.Comparison);

	DisplayName += PCGEx::GetSelectorDisplayName(Config.OperandB);
	DisplayName += TEXT(" (");

	switch (Config.Adjacency.Mode)
	{
	case EPCGExAdjacencyTestMode::All:
		DisplayName += TEXT("All");
		break;
	case EPCGExAdjacencyTestMode::Some:
		DisplayName += TEXT("Some");
		break;
	default: ;
	}

	DisplayName += TEXT(")");
	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
