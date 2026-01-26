// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/FloodFill/FillControls/PCGExFillControlHeuristicsThreshold.h"

#include "Clusters/PCGExCluster.h"
#include "Containers/PCGExHashLookup.h"
#include "Containers/PCGExManagedObjects.h"
#include "Details/PCGExSettingsDetails.h"

PCGEX_SETTING_VALUE_IMPL(FPCGExFillControlConfigHeuristicsThreshold, Threshold, double, ThresholdInput, ThresholdAttribute, Threshold)

bool FPCGExFillControlHeuristicsThreshold::PrepareForDiffusions(FPCGExContext* InContext, const TSharedPtr<PCGExFloodFill::FFillControlsHandler>& InHandler)
{
	if (!FPCGExFillControlOperation::PrepareForDiffusions(InContext, InHandler)) { return false; }

	const UPCGExFillControlsFactoryHeuristicsThreshold* TypedFactory = Cast<UPCGExFillControlsFactoryHeuristicsThreshold>(Factory);

	ThresholdSource = TypedFactory->Config.ThresholdSource;
	Comparison = TypedFactory->Config.Comparison;
	Tolerance = TypedFactory->Config.Tolerance;

	// Initialize threshold setting value
	Threshold = TypedFactory->Config.GetValueSettingThreshold();
	if (!Threshold->Init(GetSourceFacade())) { return false; }

	if (TypedFactory->HeuristicsFactories.IsEmpty())
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Heuristics Threshold fill control requires at least one heuristic."));
		return false;
	}

	// Build our own heuristics handler
	HeuristicsHandler = MakeShared<PCGExHeuristics::FHandler>(
		InContext,
		InHandler->VtxDataFacade,
		InHandler->EdgeDataFacade,
		TypedFactory->HeuristicsFactories);

	HeuristicsHandler->PrepareForCluster(InHandler->Cluster);
	HeuristicsHandler->CompleteClusterPreparation();

	return true;
}

void FPCGExFillControlHeuristicsThreshold::ScoreCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& From, PCGExFloodFill::FCandidate& OutCandidate)
{
	if (!HeuristicsHandler) { return; }

	const PCGExClusters::FNode& FromNode = *From.Node;
	const PCGExClusters::FNode& ToNode = *OutCandidate.Node;
	const PCGExClusters::FNode& SeedNode = *Diffusion->SeedNode;
	const PCGExClusters::FNode& RoamingGoal = *HeuristicsHandler->GetRoamingGoal();

	// Cast TravelStack to base class type for parameter compatibility
	const TSharedPtr<PCGEx::FHashLookup> TravelStack = Diffusion->TravelStack;

	// Compute and cache edge score (always needed for threshold check)
	LastComputedEdgeScore = HeuristicsHandler->GetEdgeScore(
		FromNode, ToNode,
		*Cluster->GetEdge(OutCandidate.Link),
		SeedNode, RoamingGoal,
		nullptr, TravelStack);

	// Compute global score if needed
	if (ThresholdSource == EPCGExFloodFillThresholdSource::GlobalScore)
	{
		LastComputedGlobalScore = HeuristicsHandler->GetGlobalScore(FromNode, SeedNode, ToNode);
	}

	// Accumulate scores for path tracking
	OutCandidate.PathScore = From.PathScore + LastComputedEdgeScore;
	OutCandidate.Score += LastComputedEdgeScore;
}

bool FPCGExFillControlHeuristicsThreshold::IsValidCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& From, const PCGExFloodFill::FCandidate& Candidate)
{
	double Value = 0;
	switch (ThresholdSource)
	{
	case EPCGExFloodFillThresholdSource::EdgeScore:
		Value = LastComputedEdgeScore;
		break;
	case EPCGExFloodFillThresholdSource::GlobalScore:
		Value = LastComputedGlobalScore;
		break;
	case EPCGExFloodFillThresholdSource::ScoreDelta:
		Value = Candidate.Score - From.Score;
		break;
	}

	const double ThresholdValue = Threshold->Read(GetSettingsIndex(Diffusion));

	return PCGExCompare::Compare(Comparison, Value, ThresholdValue, Tolerance);
}

TSharedPtr<FPCGExFillControlOperation> UPCGExFillControlsFactoryHeuristicsThreshold::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(FillControlHeuristicsThreshold)
	PCGEX_FORWARD_FILLCONTROL_OPERATION
	return NewOperation;
}

void UPCGExFillControlsFactoryHeuristicsThreshold::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);

	for (const TObjectPtr<const UPCGExHeuristicsFactoryData>& HFactory : HeuristicsFactories)
	{
		HFactory->RegisterBuffersDependencies(InContext, FacadePreloader);
	}
}

TArray<FPCGPinProperties> UPCGExFillControlsHeuristicsThresholdProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_FACTORIES(PCGExHeuristics::Labels::SourceHeuristicsLabel, "Heuristics used for threshold calculation.", Required, FPCGExDataTypeInfoHeuristics::AsId())
	return PinProperties;
}

UPCGExFactoryData* UPCGExFillControlsHeuristicsThresholdProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExFillControlsFactoryHeuristicsThreshold* NewFactory = InContext->ManagedObjects->New<UPCGExFillControlsFactoryHeuristicsThreshold>();
	PCGEX_FORWARD_FILLCONTROL_FACTORY
	Super::CreateFactory(InContext, NewFactory);

	if (!PCGExFactories::GetInputFactories(InContext, PCGExHeuristics::Labels::SourceHeuristicsLabel, NewFactory->HeuristicsFactories, {PCGExFactories::EType::Heuristics}))
	{
		InContext->ManagedObjects->Destroy(NewFactory);
		return nullptr;
	}

	return NewFactory;
}

#if WITH_EDITOR
FString UPCGExFillControlsHeuristicsThresholdProviderSettings::GetDisplayName() const
{
	return GetDefaultNodeTitle().ToString().Replace(TEXT("PCGEx | Fill Control"), TEXT("FC"));
}
#endif
