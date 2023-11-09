// Fill out your copyright notice in the Description page of Project Settings.

#include "Relational/PCGExRelationsParamsProcessor.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "Relational/PCGExRelationsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExRelationalSettings"

#pragma region UPCGSettings interface

namespace PCGExRelational
{
	const FName SourcePointsLabel = TEXT("Source");
	const FName SourceRelationalParamsLabel = TEXT("RelationalParams");
	const FName OutputPointsLabel = TEXT("Points");
}

#if WITH_EDITOR

FText UPCGExRelationsProcessorSettings::GetNodeTooltipText() const
{
	return LOCTEXT("PCGDirectionalRelationshipsTooltip", "Write the current point index to an attribute.");
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGExRelationsProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	FPCGPinProperties& PinPropertySource = PinProperties.Emplace_GetRef(PCGExRelational::SourcePointsLabel, EPCGDataType::Point);
	FPCGPinProperties& PinPropertyParams = PinProperties.Emplace_GetRef(PCGExRelational::SourceRelationalParamsLabel, EPCGDataType::Param);

#if WITH_EDITOR
	PinPropertySource.Tooltip = LOCTEXT("PCGExSourcePinTooltip", "For each of the source points, their index position in the data will be written to an attribute.");
	PinPropertyParams.Tooltip = LOCTEXT("PCGExRelationalParamsPinTooltip", "Relational Params.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExRelationsProcessorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPointsOutput = PinProperties.Emplace_GetRef(PCGExRelational::OutputPointsLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPointsOutput.Tooltip = LOCTEXT("PCGExOutputPointsPinTooltip", "The source points.");
#endif // WITH_EDITOR

	return PinProperties;
}

FPCGContext* FPCGExRelationsProcessorElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExRelationsProcessorContext* Context = InitializeRelationsContext<FPCGExRelationsProcessorContext>(InputData, SourceComponent, Node);		
	return Context;
}

#pragma endregion

bool FPCGExRelationsProcessorElement::ExecuteInternal(FPCGContext* Context) const
{
	return true;
}

#undef LOCTEXT_NAMESPACE
