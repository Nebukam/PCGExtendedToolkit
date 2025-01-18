// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Shapes/PCGExShapeProcessor.h"

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
		if (GetMainAcceptMultipleData()) { PCGEX_PIN_POINTS(GetMainInputPin(), "The point data to be processed.", Required, {}) }
		else { PCGEX_PIN_POINT(GetMainInputPin(), "The point data to be processed.", Required, {}) }
	}

	PCGEX_PIN_FACTORIES(PCGExShapes::SourceShapeBuildersLabel, "Shape builders that will be used by this element.", Required, {})

	if (SupportsPointFilters())
	{
		if (RequiresPointFilters()) { PCGEX_PIN_FACTORIES(GetPointFilterPin(), GetPointFilterTooltip(), Required, {}) }
		else { PCGEX_PIN_FACTORIES(GetPointFilterPin(), GetPointFilterTooltip(), Normal, {}) }
	}

	return PinProperties;
}

PCGExData::EIOInit UPCGExShapeProcessorSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

PCGEX_INITIALIZE_CONTEXT(ShapeProcessor)

bool FPCGExShapeProcessorElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ShapeProcessor)

	if (!PCGExFactories::GetInputFactories(
		Context, PCGExShapes::SourceShapeBuildersLabel, Context->BuilderFactories,
		{PCGExFactories::EType::ShapeBuilder}, true))
	{
		return false;
	}

	return true;
}


#undef LOCTEXT_NAMESPACE
