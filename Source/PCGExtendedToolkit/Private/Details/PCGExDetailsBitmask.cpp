// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExDetailsBitmask.h"

#include "PCGExContext.h"
#include "Collections/PCGExBitmaskCollection.h"

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

void FPCGExClampedBitOp::Mutate(int64& Flags) const
{
	const int64 BitMask = static_cast<int64>(1) << BitIndex;
	switch (Op)
	{
	case EPCGExBitOp::Set: if (bValue) { Flags |= BitMask; }
		else { Flags &= ~BitMask; }
		break;
	case EPCGExBitOp::AND: if (!bValue) { Flags &= ~BitMask; } // AND false -> clear bit 
		// AND true -> leave bit as is, nothing to do
		break;
	case EPCGExBitOp::OR: if (bValue) { Flags |= BitMask; } // OR true -> set bit 
		// OR false -> leave bit as is, nothing to do
		break;
	case EPCGExBitOp::NOT: if (bValue) { Flags ^= BitMask; } // flip the bit
		break;
	case EPCGExBitOp::XOR: if (bValue) { Flags ^= BitMask; } // XOR with true flips the bit
		// XOR false -> do nothing
		break;
	default: break;
	}
}

#if WITH_EDITOR
TArray<FName> FPCGExBitmaskRef::EDITOR_GetIdentifierOptions() const
{
	if (Source) { return Source->EDITOR_GetIdentifierOptions(); }
	return {TEXT("INVALID")};
}
#endif

void FPCGExBitmaskRef::EDITOR_RegisterTrackingKeys(FPCGExContext* Context) const
{
	if (Source) { Context->EDITOR_TrackPath(Source); }
}

void FPCGExBitmaskRef::Mutate(int64& Flags) const
{
	int64 Mask = 0;
	if (Source && Source->LoadCache()->TryGetBitmask(Identifier, Mask)) { PCGExBitmask::Mutate(Op, Flags, Mask); }
}

FPCGExSimpleBitmask FPCGExBitmaskRef::GetSimpleBitmask() const
{
	FPCGExSimpleBitmask SimpleBitmask;
	if (Source && Source->LoadCache()->TryGetBitmask(Identifier, SimpleBitmask.Bitmask)) { SimpleBitmask.Op = Op; }
	else { SimpleBitmask.Op = EPCGExBitOp::OR; }
	return SimpleBitmask;
}

bool FPCGExBitmaskRef::TryGetAdjacencyInfos(FVector& OutDirection, FPCGExSimpleBitmask& OutSimpleBitmask) const
{
	if (PCGExBitmask::FCachedRef Cache = PCGExBitmask::FCachedRef{}; Source && Source->LoadCache()->TryGetBitmask(Identifier, Cache))
	{
		OutSimpleBitmask = FPCGExSimpleBitmask();
		OutSimpleBitmask.Bitmask = Cache.Bitmask;
		OutSimpleBitmask.Op = Op;

		OutDirection = Cache.Direction;

		return true;
	}

	return false;
}

int64 FPCGExBitmask::Get() const
{
	int64 Mask = Bitmask;
	if (Mode == EPCGExBitmaskMode::Individual) { for (const FPCGExClampedBitOp& Bit : Mutations) { Bit.Mutate(Mask); } }
	PCGExBitmask::Mutate(Compositions, Mask);
	return Mask;
}

void FPCGExBitmask::EDITOR_RegisterTrackingKeys(FPCGExContext* Context) const
{
	for (const FPCGExBitmaskRef& Comp : Compositions) { Comp.EDITOR_RegisterTrackingKeys(Context); }
}

#if WITH_EDITOR
void FPCGExBitmask::ApplyDeprecation()
{
	if (Mode == EPCGExBitmaskMode::Composite)
	{
		Bitmask = 0;
		Bitmask |= static_cast<int64>(Range_00_08_DEPRECATED) << 0;
		Bitmask |= static_cast<int64>(Range_08_16_DEPRECATED) << 8;
		Bitmask |= static_cast<int64>(Range_16_24_DEPRECATED) << 16;
		Bitmask |= static_cast<int64>(Range_24_32_DEPRECATED) << 24;
		Bitmask |= static_cast<int64>(Range_32_40_DEPRECATED) << 32;
		Bitmask |= static_cast<int64>(Range_40_48_DEPRECATED) << 40;
		Bitmask |= static_cast<int64>(Range_48_56_DEPRECATED) << 48;
		Bitmask |= static_cast<int64>(Range_56_64_DEPRECATED) << 56;
	}
	else if (Mode == EPCGExBitmaskMode::Individual)
	{
		Bitmask = 0;
		Mutations.Reserve(Bits.Num());
		for (const FPCGExClampedBit& Bit : Bits)
		{
			if (Bit.bValue) { Bitmask |= (static_cast<int64>(1) << Bit.BitIndex); }
			FPCGExClampedBitOp& Mutation = Mutations.Emplace_GetRef();
			Mutation.BitIndex = Bit.BitIndex;
			Mutation.bValue = Bit.bValue;
			Mutation.Op = EPCGExBitOp::Set;
		}
		Bits.Empty();
	}

	Mode = EPCGExBitmaskMode::Direct;
}
#endif

int64 FPCGExBitmaskWithOperation::Get() const
{
	int64 Mask = Bitmask;
	if (Mode == EPCGExBitmaskMode::Individual) { for (const FPCGExClampedBitOp& Bit : Mutations) { Bit.Mutate(Mask); } }
	PCGExBitmask::Mutate(Compositions, Mask);

	return Mask;
}

void FPCGExBitmaskWithOperation::Mutate(int64& Flags) const
{
	if (Mode == EPCGExBitmaskMode::Individual)
	{
		for (const FPCGExClampedBitOp& Bit : Mutations) { Bit.Mutate(Flags); }
		PCGExBitmask::Mutate(Compositions, Flags);
		return;
	}

	PCGExBitmask::Mutate(Op, Flags, Get());
}

void FPCGExBitmaskWithOperation::EDITOR_RegisterTrackingKeys(FPCGExContext* Context) const
{
	for (const FPCGExBitmaskRef& Comp : Compositions) { Comp.EDITOR_RegisterTrackingKeys(Context); }
}

#if WITH_EDITOR
void FPCGExBitmaskWithOperation::ApplyDeprecation()
{
	if (Mode == EPCGExBitmaskMode::Composite)
	{
		Bitmask = 0;
		Bitmask |= static_cast<int64>(Range_00_08_DEPRECATED) << 0;
		Bitmask |= static_cast<int64>(Range_08_16_DEPRECATED) << 8;
		Bitmask |= static_cast<int64>(Range_16_24_DEPRECATED) << 16;
		Bitmask |= static_cast<int64>(Range_24_32_DEPRECATED) << 24;
		Bitmask |= static_cast<int64>(Range_32_40_DEPRECATED) << 32;
		Bitmask |= static_cast<int64>(Range_40_48_DEPRECATED) << 40;
		Bitmask |= static_cast<int64>(Range_48_56_DEPRECATED) << 48;
		Bitmask |= static_cast<int64>(Range_56_64_DEPRECATED) << 56;
		Mode = EPCGExBitmaskMode::Direct;
	}
}
#endif
