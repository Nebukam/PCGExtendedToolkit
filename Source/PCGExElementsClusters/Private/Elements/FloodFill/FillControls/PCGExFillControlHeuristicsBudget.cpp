// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/FloodFill/FillControls/PCGExFillControlHeuristicsBudget.h"

#include "Clusters/PCGExCluster.h"
#include "Containers/PCGExHashLookup.h"
#include "Containers/PCGExManagedObjects.h"
#include "Details/PCGExSettingsDetails.h"

PCGEX_SETTING_VALUE_IMPL(FPCGExFillControlConfigHeuristicsBudget, MaxBudget, double, MaxBudgetInput, MaxBudgetAttribute, MaxBudget)

bool FPCGExFillControlHeuristicsBudget::PrepareForDiffusions(FPCGExContext* InContext, const TSharedPtr<PCGExFloodFill::FFillControlsHandler>& InHandler)
{
	if (!FPCGExFillControlOperation::PrepareForDiffusions(InContext, InHandler)) { return false; }

	const UPCGExFillControlsFactoryHeuristicsBudget* TypedFactory = Cast<UPCGExFillControlsFactoryHeuristicsBudget>(Factory);

	BudgetSource = TypedFactory->Config.BudgetSource;

	// Initialize budget setting value
	MaxBudget = TypedFactory->Config.GetValueSettingMaxBudget();
	if (!MaxBudget->Init(GetSourceFacade())) { return false; }

	if (TypedFactory->HeuristicsFactories.IsEmpty())
	{
		// No heuristics provided - we'll use PathDistance as fallback for scoring
		return true;
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

void FPCGExFillControlHeuristicsBudget::ScoreCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& From, PCGExFloodFill::FCandidate& OutCandidate)
{
	if (!HeuristicsHandler)
	{
		// No heuristics - use edge distance as score
		OutCandidate.PathScore = From.PathScore + OutCandidate.Distance;
		OutCandidate.Score += OutCandidate.Distance;
		return;
	}

	const PCGExClusters::FNode& FromNode = *From.Node;
	const PCGExClusters::FNode& ToNode = *OutCandidate.Node;
	const PCGExClusters::FNode& SeedNode = *Diffusion->SeedNode;
	const PCGExClusters::FNode& RoamingGoal = *HeuristicsHandler->GetRoamingGoal();

	// Cast TravelStack to base class type for parameter compatibility
	const TSharedPtr<PCGEx::FHashLookup> TravelStack = Diffusion->TravelStack;
	const double EdgeScore = HeuristicsHandler->GetEdgeScore(
		FromNode, ToNode,
		*Cluster->GetEdge(OutCandidate.Link),
		SeedNode, RoamingGoal,
		nullptr, TravelStack);

	// Always accumulate for budget tracking
	OutCandidate.PathScore = From.PathScore + EdgeScore;
	OutCandidate.Score += EdgeScore;
}

bool FPCGExFillControlHeuristicsBudget::IsValidCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& From, const PCGExFloodFill::FCandidate& Candidate)
{
	const double CurrentValue = GetBudgetValue(Candidate);
	const double Budget = MaxBudget->Read(GetSettingsIndex(Diffusion));
	return CurrentValue <= Budget;
}

double FPCGExFillControlHeuristicsBudget::GetBudgetValue(const PCGExFloodFill::FCandidate& Candidate) const
{
	switch (BudgetSource)
	{
	case EPCGExFloodFillBudgetSource::PathScore:
		return Candidate.PathScore;
	case EPCGExFloodFillBudgetSource::CompositeScore:
		return Candidate.Score;
	case EPCGExFloodFillBudgetSource::PathDistance:
		return Candidate.PathDistance;
	}
	return 0;
}

TSharedPtr<FPCGExFillControlOperation> UPCGExFillControlsFactoryHeuristicsBudget::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(FillControlHeuristicsBudget)
	PCGEX_FORWARD_FILLCONTROL_OPERATION
	return NewOperation;
}

void UPCGExFillControlsFactoryHeuristicsBudget::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);

	for (const TObjectPtr<const UPCGExHeuristicsFactoryData>& HFactory : HeuristicsFactories)
	{
		HFactory->RegisterBuffersDependencies(InContext, FacadePreloader);
	}
}

TArray<FPCGPinProperties> UPCGExFillControlsHeuristicsBudgetProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_FACTORIES(PCGExHeuristics::Labels::SourceHeuristicsLabel, "Heuristics used for cost calculation.", Normal, FPCGExDataTypeInfoHeuristics::AsId())
	return PinProperties;
}

UPCGExFactoryData* UPCGExFillControlsHeuristicsBudgetProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExFillControlsFactoryHeuristicsBudget* NewFactory = InContext->ManagedObjects->New<UPCGExFillControlsFactoryHeuristicsBudget>();
	PCGEX_FORWARD_FILLCONTROL_FACTORY
	Super::CreateFactory(InContext, NewFactory);

	// Heuristics are optional for budget - can use PathDistance if not provided
	PCGExFactories::GetInputFactories(InContext, PCGExHeuristics::Labels::SourceHeuristicsLabel, NewFactory->HeuristicsFactories, {PCGExFactories::EType::Heuristics}, false);

	return NewFactory;
}

#if WITH_EDITOR
FString UPCGExFillControlsHeuristicsBudgetProviderSettings::GetDisplayName() const
{
	return GetDefaultNodeTitle().ToString().Replace(TEXT("PCGEx | Fill Control"), TEXT("FC"));
}
#endif
