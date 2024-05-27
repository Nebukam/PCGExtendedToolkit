// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingProcessor.h"

#include "Graph/PCGExGraph.h"
#include "PCGExPathfinding.cpp"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPicker.h"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"
#include "Graph/Pathfinding/Search/PCGExSearchAStar.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExPathfindingSettings"
#define PCGEX_NAMESPACE PathfindingProcessor

#pragma region UPCGSettings interface

TArray<FPCGPinProperties> UPCGExPathfindingProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	if (GetRequiresSeeds())
	{
		FPCGPinProperties& PinPropertySeeds = PinProperties.Emplace_GetRef(PCGExPathfinding::SourceSeedsLabel, EPCGDataType::Point, false, false);

#if WITH_EDITOR
		PinPropertySeeds.Tooltip = FTEXT("Seeds points for pathfinding.");
#endif
	}

	if (GetRequiresGoals())
	{
		FPCGPinProperties& PinPropertyGoals = PinProperties.Emplace_GetRef(PCGExPathfinding::SourceGoalsLabel, EPCGDataType::Point, false, false);

#if WITH_EDITOR
		PinPropertyGoals.Tooltip = FTEXT("Goals points for pathfinding.");
#endif
	}

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPathfindingProcessorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPathsOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputPathsLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPathsOutput.Tooltip = FTEXT("Paths output.");
#endif

	return PinProperties;
}

void UPCGExPathfindingProcessorSettings::PostInitProperties()
{
	Super::PostInitProperties();
	PCGEX_OPERATION_DEFAULT(GoalPicker, UPCGExGoalPickerRandom)
	PCGEX_OPERATION_DEFAULT(SearchAlgorithm, UPCGExSearchAStar)
	PCGEX_OPERATION_DEFAULT(Heuristics, UPCGExHeuristicDistance)
}

#pragma endregion

#if WITH_EDITOR
void UPCGExPathfindingProcessorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (GoalPicker) { GoalPicker->UpdateUserFacingInfos(); }
	if (SearchAlgorithm) { SearchAlgorithm->UpdateUserFacingInfos(); }
	if (Heuristics) { Heuristics->UpdateUserFacingInfos(); }
	//if (Blending) { Blending->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGExData::EInit UPCGExPathfindingProcessorSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }
bool UPCGExPathfindingProcessorSettings::GetRequiresSeeds() const { return true; }
bool UPCGExPathfindingProcessorSettings::GetRequiresGoals() const { return true; }

PCGEX_INITIALIZE_CONTEXT(PathfindingProcessor)

FPCGExPathfindingProcessorContext::~FPCGExPathfindingProcessorContext()
{
	PCGEX_TERMINATE_ASYNC

	if (HeuristicsModifiers) { HeuristicsModifiers->Cleanup(); }

	PCGEX_DELETE(GlobalExtraWeights)
	PCGEX_DELETE(SeedsPoints)
	PCGEX_DELETE(GoalsPoints)
	PCGEX_DELETE(OutputPaths)

	PCGEX_DELETE(SeedTagValueGetter)
	PCGEX_DELETE(GoalTagValueGetter)

	PCGEX_DELETE(SeedForwardHandler)
	PCGEX_DELETE(GoalForwardHandler)

	ProjectionSettings.Cleanup();
}

bool FPCGExPathfindingProcessorElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingProcessor)

	PCGEX_OPERATION_BIND(GoalPicker, UPCGExGoalPickerRandom)
	PCGEX_OPERATION_BIND(SearchAlgorithm, UPCGExSearchAStar)
	PCGEX_OPERATION_BIND(Heuristics, UPCGExHeuristicDistance)
	//PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInterpolate)

	if (Settings->GetRequiresSeeds())
	{
		if (!Context->SeedsPoints || Context->SeedsPoints->GetNum() == 0)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Input Seeds."));
			return false;
		}
	}

	if (Settings->GetRequiresGoals())
	{
		if (!Context->GoalsPoints || Context->GoalsPoints->GetNum() == 0)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Input Goals."));
			return false;
		}
	}

	Context->HeuristicsModifiers = const_cast<FPCGExHeuristicModifiersSettings*>(&Settings->HeuristicsModifiers);
	Context->HeuristicsModifiers->LoadCurves();
	Context->Heuristics->ReferenceWeight = Context->HeuristicsModifiers->ReferenceWeight;

	if (Settings->bUseSeedAttributeToTagPath)
	{
		Context->SeedTagValueGetter = new PCGEx::FLocalToStringGetter();
		Context->SeedTagValueGetter->Capture(Settings->SeedTagAttribute);
		if (!Context->SeedTagValueGetter->SoftGrab(*Context->SeedsPoints))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Missing specified Attribute to Tag on Seed points."));
			return false;
		}
	}

	if (Settings->bUseGoalAttributeToTagPath)
	{
		Context->GoalTagValueGetter = new PCGEx::FLocalToStringGetter();
		Context->GoalTagValueGetter->Capture(Settings->GoalTagAttribute);
		if (!Context->GoalTagValueGetter->SoftGrab(*Context->GoalsPoints))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Missing specified Attribute to Tag on Goal points."));
			return false;
		}
	}

	PCGEX_FWD(ProjectionSettings)

	Context->SeedForwardHandler = new PCGExDataBlending::FDataForwardHandler(&Settings->SeedForwardAttributes, Context->SeedsPoints);
	Context->GoalForwardHandler = new PCGExDataBlending::FDataForwardHandler(&Settings->GoalForwardAttributes, Context->GoalsPoints);

	return true;
}

FPCGContext* FPCGExPathfindingProcessorElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExPathfindingProcessorContext* Context = static_cast<FPCGExPathfindingProcessorContext*>(FPCGExEdgesProcessorElement::InitializeContext(InContext, InputData, SourceComponent, Node));

	const UPCGExPathfindingProcessorSettings* Settings = InContext->GetInputSettings<UPCGExPathfindingProcessorSettings>();
	check(Settings);

	if (!Settings->bEnabled) { return Context; }

	if (Settings->GetRequiresSeeds())
	{
		if (TArray<FPCGTaggedData> Seeds = InContext->InputData.GetInputsByPin(PCGExPathfinding::SourceSeedsLabel);
			Seeds.Num() > 0)
		{
			const FPCGTaggedData& SeedsSource = Seeds[0];
			Context->SeedsPoints = PCGExData::PCGExPointIO::GetPointIO(Context, SeedsSource);
		}
	}

	if (Settings->GetRequiresGoals())
	{
		if (TArray<FPCGTaggedData> Goals = InContext->InputData.GetInputsByPin(PCGExPathfinding::SourceGoalsLabel);
			Goals.Num() > 0)
		{
			const FPCGTaggedData& GoalsSource = Goals[0];
			Context->GoalsPoints = PCGExData::PCGExPointIO::GetPointIO(Context, GoalsSource);
		}
	}

	Context->OutputPaths = new PCGExData::FPointIOCollection();

	PCGEX_FWD(bAddSeedToPath)
	PCGEX_FWD(bAddGoalToPath)

	return Context;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
