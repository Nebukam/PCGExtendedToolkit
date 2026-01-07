// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathIntersectionDetails.h"

void FPCGExPathEdgeIntersectionDetails::Init()
{
	MaxDot = bUseMinAngle ? PCGExMath::DegreesToDot(MinAngle) : 1;
	MinDot = bUseMaxAngle ? PCGExMath::DegreesToDot(MaxAngle) : -1;
	ToleranceSquared = Tolerance * Tolerance;
}

void FPCGExPathFilterSettings::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
}

bool FPCGExPathFilterSettings::Init(FPCGExContext* InContext)
{
	return true;
}

FPCGExPathIntersectionDetails::FPCGExPathIntersectionDetails(const double InTolerance, const double InMinAngle, const double InMaxAngle)
{
	Tolerance = InTolerance;
	MinAngle = InMinAngle;
	MaxAngle = InMaxAngle;
	bUseMinAngle = InMinAngle > 0;
	bUseMaxAngle = InMaxAngle < 90;
}

void FPCGExPathIntersectionDetails::Init()
{
	MaxDot = bUseMinAngle ? PCGExMath::DegreesToDot(MinAngle) : 1;
	MinDot = bUseMaxAngle ? PCGExMath::DegreesToDot(MaxAngle) : -1;
	ToleranceSquared = Tolerance * Tolerance;
	bWantsDotCheck = bUseMinAngle || bUseMaxAngle;
}
