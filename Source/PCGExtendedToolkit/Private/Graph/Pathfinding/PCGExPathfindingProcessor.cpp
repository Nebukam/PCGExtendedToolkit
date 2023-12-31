﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingProcessor.h"

#include "Graph/PCGExGraph.h"
#include "PCGExPathfinding.cpp"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPicker.h"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExPathfindingSettings"
#define PCGEX_NAMESPACE PathfindingProcessor

#pragma region UPCGSettings interface

UPCGExPathfindingProcessorSettings::UPCGExPathfindingProcessorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PCGEX_OPERATION_DEFAULT(GoalPicker, UPCGExGoalPickerRandom)
	PCGEX_OPERATION_DEFAULT(Heuristics, UPCGExHeuristicDistance)
}

TArray<FPCGPinProperties> UPCGExPathfindingProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	if (GetRequiresSeeds())
	{
		FPCGPinProperties& PinPropertySeeds = PinProperties.Emplace_GetRef(PCGExPathfinding::SourceSeedsLabel, EPCGDataType::Point, false, false);

#if WITH_EDITOR
		PinPropertySeeds.Tooltip = FTEXT("Seeds points for pathfinding.");
#endif // WITH_EDITOR
	}

	if (GetRequiresGoals())
	{
		FPCGPinProperties& PinPropertyGoals = PinProperties.Emplace_GetRef(PCGExPathfinding::SourceGoalsLabel, EPCGDataType::Point, false, false);

#if WITH_EDITOR
		PinPropertyGoals.Tooltip = FTEXT("Goals points for pathfinding.");
#endif // WITH_EDITOR
	}

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPathfindingProcessorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPathsOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputPathsLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPathsOutput.Tooltip = FTEXT("Paths output.");
#endif // WITH_EDITOR

	return PinProperties;
}

#pragma endregion

#if WITH_EDITOR
void UPCGExPathfindingProcessorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (GoalPicker) { GoalPicker->UpdateUserFacingInfos(); }
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

	PCGEX_DELETE(SeedsPoints)
	PCGEX_DELETE(GoalsPoints)
	PCGEX_DELETE(OutputPaths)
}

bool FPCGExPathfindingProcessorElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingProcessor)

	PCGEX_OPERATION_BIND(GoalPicker, UPCGExGoalPickerRandom)
	PCGEX_OPERATION_BIND(Heuristics, UPCGExHeuristicDistance)
	//PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInterpolate)

	if (Settings->GetRequiresSeeds() && !Context->SeedsPoints)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Input Seeds."));
		return false;
	}

	if (Settings->GetRequiresGoals() && !Context->GoalsPoints)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Input Goals."));
		return false;
	}

	Context->HeuristicsModifiers = const_cast<FPCGExHeuristicModifiersSettings*>(&Settings->HeuristicsModifiers);

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

	Context->OutputPaths = new PCGExData::FPointIOGroup();

	PCGEX_FWD(bAddSeedToPath)
	PCGEX_FWD(bAddGoalToPath)

	return Context;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
