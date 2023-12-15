// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingProcessor.h"

#include "Graph/Pathfinding/PCGExPathfinding.h"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPicker.h"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendOperation.h"
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

void UPCGExPathfindingProcessorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (GoalPicker) { GoalPicker->UpdateUserFacingInfos(); }
	if (Blending) { Blending->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

PCGExData::EInit UPCGExPathfindingProcessorSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }
bool UPCGExPathfindingProcessorSettings::GetRequiresSeeds() const { return true; }
bool UPCGExPathfindingProcessorSettings::GetRequiresGoals() const { return true; }

FPCGExPathfindingProcessorContext::~FPCGExPathfindingProcessorContext()
{
	PCGEX_DELETE(SeedsPoints)
	PCGEX_DELETE(GoalsPoints)
	PCGEX_DELETE(OutputPaths)
}

PCGEX_INITIALIZE_CONTEXT(PathfindingProcessor)

bool FPCGExPathfindingProcessorElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }
	
	PCGEX_CONTEXT_AND_SETTINGS(PathfindingProcessor)

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

	Context->GoalPicker = Settings->EnsureOperation<UPCGExGoalPickerRandom>(Settings->GoalPicker, Context);
	Context->Blending = Settings->EnsureOperation<UPCGExSubPointsBlendInterpolate>(Settings->Blending, Context);

	Context->bAddSeedToPath = Settings->bAddSeedToPath;
	Context->bAddGoalToPath = Settings->bAddGoalToPath;

	return Context;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
