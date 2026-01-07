// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExRemapDetails.h"


void FPCGExRemapDetails::Init()
{
	RemapLUT = RemapCurveLookup.MakeLookup(bUseLocalCurve, LocalScoreCurve, RemapCurve);
}

double FPCGExRemapDetails::GetRemappedValue(const double Value, const double Step) const
{
	switch (Snapping)
	{
	default: case EPCGExVariationSnapping::None: return PCGExMath::TruncateDbl(RemapLUT->Eval(PCGExMath::Remap(Value, InMin, InMax, 0, 1)) * Scale, TruncateOutput) * PostTruncateScale + Offset;
	case EPCGExVariationSnapping::SnapOffset:
		{
			double V = PCGExMath::TruncateDbl(RemapLUT->Eval(PCGExMath::Remap(Value, InMin, InMax, 0, 1)) * Scale, TruncateOutput) * PostTruncateScale;
			PCGExMath::Snap(V, Step);
			return V + Offset;
		}
	case EPCGExVariationSnapping::SnapResult:
		{
			double V = PCGExMath::TruncateDbl(RemapLUT->Eval(PCGExMath::Remap(Value, InMin, InMax, 0, 1)) * Scale, TruncateOutput) * PostTruncateScale + Offset;
			PCGExMath::Snap(V, Step);
			return V;
		}
	}
}
