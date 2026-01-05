// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExClipper2Boolean.h"

#include "Data/PCGExPointIO.h"
#include "Clipper2Lib/clipper.h"
#include "Details/PCGExBlendingDetails.h"

#define LOCTEXT_NAMESPACE "PCGExClipper2BooleanElement"
#define PCGEX_NAMESPACE Clipper2Boolean

PCGEX_INITIALIZE_ELEMENT(Clipper2Boolean)

bool UPCGExClipper2BooleanSettings::NeedsOperands() const
{
	return bUseOperandsPin || Operation == EPCGExClipper2BooleanOp::Difference;
}

FPCGExGeo2DProjectionDetails UPCGExClipper2BooleanSettings::GetProjectionDetails() const
{
	return ProjectionDetails;
}

void FPCGExClipper2BooleanContext::Process(const TSharedPtr<PCGExClipper2::FProcessingGroup>& Group)
{
	const UPCGExClipper2BooleanSettings* Settings = GetInputSettings<UPCGExClipper2BooleanSettings>();

	if (!Group->IsValid()) { return; }

	// Create clipper and set up ZCallback for intersection tracking
	PCGExClipper2Lib::Clipper64 Clipper;
	Clipper.SetZCallback(Group->CreateZCallback());

	// Add subject paths
	if (!Group->SubjectPaths.empty()) { Clipper.AddSubject(Group->SubjectPaths); }
	if (!Group->OpenSubjectPaths.empty()) { Clipper.AddOpenSubject(Group->OpenSubjectPaths); }

	// Add operand paths as clips if available
	if (!Group->OperandPaths.empty()) { Clipper.AddClip(Group->OperandPaths); }
	if (!Group->OpenOperandPaths.empty()) { Clipper.AddClip(Group->OpenOperandPaths); }

	// Determine clip type
	PCGExClipper2Lib::ClipType ClipType;
	switch (Settings->Operation)
	{
	case EPCGExClipper2BooleanOp::Intersection:
		ClipType = PCGExClipper2Lib::ClipType::Intersection;
		break;
	case EPCGExClipper2BooleanOp::Union:
		ClipType = PCGExClipper2Lib::ClipType::Union;
		break;
	case EPCGExClipper2BooleanOp::Difference:
		ClipType = PCGExClipper2Lib::ClipType::Difference;
		break;
	case EPCGExClipper2BooleanOp::Xor:
		ClipType = PCGExClipper2Lib::ClipType::Xor;
		break;
	default:
		ClipType = PCGExClipper2Lib::ClipType::Union;
		break;
	}

	// Execute the boolean operation
	PCGExClipper2Lib::Paths64 ResultPaths;
	const PCGExClipper2Lib::FillRule FillRule = PCGExClipper2Lib::FillRule::NonZero;

	if (!Clipper.Execute(ClipType, FillRule, ResultPaths)) { return; }

	if (ResultPaths.empty()) { return; }

	// Convert results back to PCGEx data with blending
	TArray<TSharedPtr<PCGExData::FPointIO>> OutputPaths;

	// Setup blending details for boolean ops
	FPCGExBlendingDetails BlendingDetails(EPCGExBlendingType::Average);
	BlendingDetails.PropertiesOverrides.bOverridePosition = true;
	BlendingDetails.PropertiesOverrides.PositionBlending = EPCGExBlendingType::None; // Position is set by source transforms

	OutputPaths64(ResultPaths, Group, &BlendingDetails, &Settings->CarryOver, OutputPaths);
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
