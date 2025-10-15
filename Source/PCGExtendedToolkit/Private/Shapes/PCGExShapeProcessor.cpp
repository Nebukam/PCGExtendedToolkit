﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Shapes/PCGExShapeProcessor.h"

#include "Data/PCGExPointFilter.h"
#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExShapeProcessorElement"

UPCGExShapeProcessorSettings::UPCGExShapeProcessorSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPCGExShapeProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	if (!IsInputless())
	{
		if (GetMainAcceptMultipleData()) { PCGEX_PIN_POINTS(GetMainInputPin(), "The point data to be processed.", Required) }
		else { PCGEX_PIN_POINT(GetMainInputPin(), "The point data to be processed.", Required) }
	}

	PCGEX_PIN_FACTORIES(PCGExShapes::SourceShapeBuildersLabel, "Shape builders that will be used by this element.", Required, FPCGExDataTypeInfoShape::AsId())

	if (SupportsPointFilters())
	{
		if (RequiresPointFilters()) { PCGEX_PIN_FILTERS(GetPointFilterPin(), GetPointFilterTooltip(), Required) }
		else { PCGEX_PIN_FILTERS(GetPointFilterPin(), GetPointFilterTooltip(), Normal) }
	}

	return PinProperties;
}

FName UPCGExShapeProcessorSettings::GetMainInputPin() const { return PCGExGraph::SourceSeedsLabel; }

bool FPCGExShapeProcessorElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ShapeProcessor)

	if (!PCGExFactories::GetInputFactories(
		Context, PCGExShapes::SourceShapeBuildersLabel, Context->BuilderFactories,
		{PCGExFactories::EType::ShapeBuilder}))
	{
		return false;
	}

	return true;
}


#undef LOCTEXT_NAMESPACE
