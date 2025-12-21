// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Bitmasks/PCGExBitmaskCommon.h"

#include "Data/Bitmasks/PCGExBitmaskDetails.h"

namespace PCGExBitmask
{
	FString ToString(const EPCGExBitflagComparison Comparison)
	{
		switch (Comparison)
		{
		case EPCGExBitflagComparison::MatchPartial: return " Any ";
		case EPCGExBitflagComparison::MatchFull: return " All ";
		case EPCGExBitflagComparison::MatchStrict: return " Exactly ";
		case EPCGExBitflagComparison::NoMatchPartial: return " Not Any ";
		case EPCGExBitflagComparison::NoMatchFull: return " Not All ";
		default: return " ?? ";
		}
	}

	bool Compare(const EPCGExBitflagComparison Method, const int64& Flags, const int64& Mask)
	{
		switch (Method)
		{
		case EPCGExBitflagComparison::MatchPartial: return ((Flags & Mask) != 0);
		case EPCGExBitflagComparison::MatchFull: return ((Flags & Mask) == Mask);
		case EPCGExBitflagComparison::MatchStrict: return (Flags == Mask);
		case EPCGExBitflagComparison::NoMatchPartial: return ((Flags & Mask) == 0);
		case EPCGExBitflagComparison::NoMatchFull: return ((Flags & Mask) != Mask);
		default: return false;
		}
	}

	void Mutate(const TArray<FPCGExBitmaskRef>& Compositions, int64& Flags)
	{
		for (const FPCGExBitmaskRef& Comp : Compositions) { Comp.Mutate(Flags); }
	}

	void Mutate(const TArray<FPCGExSimpleBitmask>& Compositions, int64& Flags)
	{
		for (const FPCGExSimpleBitmask& Comp : Compositions) { Comp.Mutate(Flags); }
	}
}
