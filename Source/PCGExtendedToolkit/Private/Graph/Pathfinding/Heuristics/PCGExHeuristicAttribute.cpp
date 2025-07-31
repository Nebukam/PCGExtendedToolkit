// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExHeuristicAttribute.h"


#include "Data/PCGExDataPreloader.h"
#include "Graph/Pathfinding/PCGExPathfinding.h"

#define LOCTEXT_NAMESPACE "PCGExCreateHeuristicAttribute"
#define PCGEX_NAMESPACE CreateHeuristicAttribute

void FPCGExHeuristicAttribute::PrepareForCluster(const TSharedPtr<const PCGExCluster::FCluster>& InCluster)
{
	FPCGExHeuristicOperation::PrepareForCluster(InCluster);

	const TSharedPtr<PCGExData::FFacade> DataFacade = Source == EPCGExClusterElement::Vtx ? PrimaryDataFacade : SecondaryDataFacade;

	const int32 NumPoints = Source == EPCGExClusterElement::Vtx ? InCluster->Nodes->Num() : InCluster->Edges->Num();
	CachedScores.SetNumZeroed(NumPoints);

	// Grab all attribute values
	const TSharedPtr<PCGExData::TBuffer<double>> Values = DataFacade->GetBroadcaster<double>(Attribute, false, true);

	if (!Values)
	{
		PCGEX_LOG_INVALID_SELECTOR_C(Context, Heuristic, Attribute)
		return;
	}

	// Grab min & max
	const double MinValue = Values->Min;
	const double MaxValue = Values->Max;

	const double OutMin = bInvert ? 1 : 0;
	const double OutMax = bInvert ? 0 : 1;

	const double Factor = ReferenceWeight * WeightFactor;

	if (MinValue == MaxValue)
	{
		// There is no value range, we can't normalize anything
		// Use desired or "auto" fallback instead.
		FallbackValue = FMath::Max(0, ScoreCurve->Eval(bUseCustomFallback ? FallbackValue : FMath::Clamp(MinValue, 0, 1))) * Factor;
		CachedScores.Init(FallbackValue, NumPoints);
	}
	else
	{
		if (Source == EPCGExClusterElement::Vtx)
		{
			for (const PCGExCluster::FNode& Node : (*InCluster->Nodes))
			{
				const double NormalizedValue = PCGExMath::Remap(Values->Read(Node.PointIndex), MinValue, MaxValue, OutMin, OutMax);
				CachedScores[Node.Index] += FMath::Max(0, ScoreCurve->Eval(NormalizedValue)) * Factor;
			}
		}
		else
		{
			for (int i = 0; i < NumPoints; i++)
			{
				const double NormalizedValue = PCGExMath::Remap(Values->Read(i), MinValue, MaxValue, OutMin, OutMax);
				CachedScores[i] += FMath::Max(0, ScoreCurve->Eval(NormalizedValue)) * Factor;
			}
		}
	}
}

double FPCGExHeuristicAttribute::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal,
	const TSharedPtr<PCGEx::FHashLookup> TravelStack) const
{
	return CachedScores[Source == EPCGExClusterElement::Edge ? Edge.PointIndex : To.Index];
}

TSharedPtr<FPCGExHeuristicOperation> UPCGExHeuristicsFactoryAttribute::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(HeuristicAttribute)
	PCGEX_FORWARD_HEURISTIC_CONFIG
	NewOperation->Source = Config.Source;
	NewOperation->Attribute = Config.Attribute;
	NewOperation->bUseCustomFallback = Config.bUseCustomFallback;
	NewOperation->FallbackValue = Config.FallbackValue;
	return NewOperation;
}

PCGEX_HEURISTIC_FACTORY_BOILERPLATE_IMPL(Attribute, {})

void UPCGExHeuristicsFactoryAttribute::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	if (Config.Source == EPCGExClusterElement::Vtx)
	{
		FacadePreloader.Register<double>(InContext, Config.Attribute);
	}
}

UPCGExFactoryData* UPCGExCreateHeuristicAttributeSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExHeuristicsFactoryAttribute* NewFactory = InContext->ManagedObjects->New<UPCGExHeuristicsFactoryAttribute>();
	PCGEX_FORWARD_HEURISTIC_FACTORY
	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExCreateHeuristicAttributeSettings::GetDisplayName() const
{
	return TEXT("HX : ") + PCGEx::GetSelectorDisplayName(Config.Attribute)
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.WeightFactor) / 1000.0));
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
