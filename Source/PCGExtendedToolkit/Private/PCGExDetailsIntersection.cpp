// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExDetailsIntersection.h"

#include "PCGExMath.h"

bool FPCGExUnionMetadataDetails::SanityCheck(FPCGExContext* InContext) const
{
	if (bWriteIsUnion) { PCGEX_VALIDATE_NAME_C(InContext, IsUnionAttributeName) }
	if (bWriteUnionSize) { PCGEX_VALIDATE_NAME_C(InContext, UnionSizeAttributeName) }
	return true;
}

bool FPCGExPointPointIntersectionDetails::SanityCheck(FPCGExContext* InContext) const
{
	if (bSupportsEdges) { if (!EdgeUnionData.SanityCheck(InContext)) { return false; } }
	if (!PointUnionData.SanityCheck(InContext)) { return false; }
	return true;
}

void FPCGExEdgeEdgeIntersectionDetails::Init()
{
	MaxDot = bUseMinAngle ? PCGExMath::DegreesToDot(MinAngle) : 1;
	MinDot = bUseMaxAngle ? PCGExMath::DegreesToDot(MaxAngle) : -1;
	ToleranceSquared = Tolerance * Tolerance;
}
