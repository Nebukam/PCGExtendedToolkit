// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExAttachmentRules.h"

FPCGExAttachmentRules::FPCGExAttachmentRules(EAttachmentRule InLoc, EAttachmentRule InRot, EAttachmentRule InScale)
	: LocationRule(InLoc), RotationRule(InRot), ScaleRule(InScale)
{
}

FAttachmentTransformRules FPCGExAttachmentRules::GetRules() const
{
	return FAttachmentTransformRules(LocationRule, RotationRule, ScaleRule, bWeldSimulatedBodies);
}
