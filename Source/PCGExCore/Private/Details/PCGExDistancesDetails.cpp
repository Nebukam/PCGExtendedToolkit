// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Details/PCGExDistancesDetails.h"

#include "Math/PCGExMathDistances.h"

const PCGExMath::FDistances* FPCGExDistanceDetails::MakeDistances() const
{
	return PCGExMath::GetDistances(Source, Target);
}
