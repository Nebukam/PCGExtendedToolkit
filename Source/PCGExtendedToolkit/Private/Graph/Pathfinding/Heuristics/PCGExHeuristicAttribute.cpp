// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExHeuristicAttribute.h"

#include "Graph/Pathfinding/PCGExPathfinding.h"

#define LOCTEXT_NAMESPACE "PCGExCreateHeuristicAttribute"
#define PCGEX_NAMESPACE CreateHeuristicAttribute

void UPCGExHeuristicAttribute::PrepareForCluster(PCGExCluster::FCluster* InCluster)
{
	Super::PrepareForCluster(InCluster);

	PCGExData::FPointIO& InPoints = Source == EPCGExGraphValueSource::Point ? *InCluster->PointsIO : *InCluster->EdgesIO;

	if (LastPoints == &InPoints) { return; }

	const int32 NumPoints = InPoints.GetNum();

	LastPoints = &InPoints;
	InPoints.CreateInKeys();
	CachedScores.SetNumZeroed(NumPoints);

	PCGEx::FLocalSingleFieldGetter* ModifierGetter = new PCGEx::FLocalSingleFieldGetter();
	ModifierGetter->Capture(Attribute);

	const bool bModifierGrabbed = ModifierGetter->Grab(InPoints, true);
	if (!bModifierGrabbed || !ModifierGetter->bValid || !ModifierGetter->bEnabled)
	{
		PCGEX_DELETE(ModifierGetter)
		PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("Invalid Heuristic attribute: {0}."), FText::FromName(Attribute.GetName())));
		return;
	}

	const double MinValue = ModifierGetter->Min;
	const double MaxValue = ModifierGetter->Max;

	const double OutMin = bInvert ? 1 : 0;
	const double OutMax = bInvert ? 0 : 1;

	const double Factor = ReferenceWeight * WeightFactor;

	for (int i = 0; i < NumPoints; i++)
	{
		const double NormalizedValue = PCGExMath::Remap(ModifierGetter->Values[i], MinValue, MaxValue, OutMin, OutMax);
		CachedScores[i] += FMath::Max(0, ScoreCurveObj->GetFloatValue(NormalizedValue)) * Factor;
	}

	PCGEX_DELETE(ModifierGetter)
}

double UPCGExHeuristicAttribute::GetGlobalScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return 0;
}

double UPCGExHeuristicAttribute::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FIndexedEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return CachedScores[Source == EPCGExGraphValueSource::Edge ? Edge.PointIndex : To.PointIndex];
}

void UPCGExHeuristicAttribute::Cleanup()
{
	CachedScores.Empty();
	Super::Cleanup();
}

UPCGExHeuristicOperation* UPCGHeuristicsFactoryAttribute::CreateOperation() const
{
	UPCGExHeuristicAttribute* NewOperation = NewObject<UPCGExHeuristicAttribute>();
	PCGEX_FORWARD_HEURISTIC_DESCRIPTOR
	NewOperation->Attribute = Descriptor.Attribute;
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExCreateHeuristicAttributeSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGHeuristicsFactoryAttribute* NewFactory = NewObject<UPCGHeuristicsFactoryAttribute>();
	PCGEX_FORWARD_HEURISTIC_FACTORY
	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExCreateHeuristicAttributeSettings::GetDisplayName() const
{
	return Descriptor.Attribute.GetName().ToString()
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Descriptor.WeightFactor) / 1000.0));
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
