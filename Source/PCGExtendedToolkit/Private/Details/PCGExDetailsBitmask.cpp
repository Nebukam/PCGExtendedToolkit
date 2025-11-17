// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExDetailsBitmask.h"

namespace PCGExBitmask
{
	FString ToString(const EPCGExBitflagComparison Comparison)
	{
		switch (Comparison)
		{
		case EPCGExBitflagComparison::MatchPartial:
			return " Any ";
		case EPCGExBitflagComparison::MatchFull:
			return " All ";
		case EPCGExBitflagComparison::MatchStrict:
			return " Exactly ";
		case EPCGExBitflagComparison::NoMatchPartial:
			return " Not Any ";
		case EPCGExBitflagComparison::NoMatchFull:
			return " Not All ";
		default:
			return " ?? ";
		}
	}

	bool Compare(const EPCGExBitflagComparison Method, const int64& Flags, const int64& Mask)
	{
		switch (Method)
		{
		case EPCGExBitflagComparison::MatchPartial:
			return ((Flags & Mask) != 0);
		case EPCGExBitflagComparison::MatchFull:
			return ((Flags & Mask) == Mask);
		case EPCGExBitflagComparison::MatchStrict:
			return (Flags == Mask);
		case EPCGExBitflagComparison::NoMatchPartial:
			return ((Flags & Mask) == 0);
		case EPCGExBitflagComparison::NoMatchFull:
			return ((Flags & Mask) != Mask);
		default: return false;
		}
	}
}

void FPCGExClampedBitOp::Mutate(int64& Flags) const
{
	const int64 BitMask = static_cast<int64>(1) << BitIndex;
	switch (Op)
	{
	case EPCGExBitOp::Set:
		if (bValue) { Flags |= BitMask; }
		else { Flags &= ~BitMask; }
		break;
	case EPCGExBitOp::AND:
		if (!bValue) { Flags &= ~BitMask; } // AND false -> clear bit 
		// AND true -> leave bit as is, nothing to do
		break;
	case EPCGExBitOp::OR:
		if (bValue) { Flags |= BitMask; } // OR true -> set bit 
		// OR false -> leave bit as is, nothing to do
		break;
	case EPCGExBitOp::NOT:
		if (bValue) { Flags ^= BitMask; } // flip the bit
		break;
	case EPCGExBitOp::XOR:
		if (bValue) { Flags ^= BitMask; } // XOR with true flips the bit
		// XOR false -> do nothing
		break;
	default:
		break;
	}
}

int64 FPCGExBitmask::Get() const
{
	if (Mode != EPCGExBitmaskMode::Individual) { return Bitmask; }

	int64 Mask = Bitmask;
	for (const FPCGExClampedBitOp& Bit : Mutations) { Bit.Mutate(Mask); }
	return Mask;
}

void FPCGExBitmask::DoOperation(const EPCGExBitOp Op, int64& Flags) const
{
	const int64 Mask = Get();
	switch (Op)
	{
	case EPCGExBitOp::Set:
		Flags = Mask;
		break;
	case EPCGExBitOp::AND:
		Flags &= Mask;
		break;
	case EPCGExBitOp::OR:
		Flags |= Mask;
		break;
	case EPCGExBitOp::NOT:
		Flags &= ~Mask;
		break;
	case EPCGExBitOp::XOR:
		Flags ^= Mask;
		break;
	default: ;
	}
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
		Mode = EPCGExBitmaskMode::Direct;
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
		UE_LOG(LogTemp, Warning, TEXT("Bits : %d"), Bits.Num())
		Bits.Empty();
		Mode = EPCGExBitmaskMode::Direct;
	}
}
#endif

int64 FPCGExBitmaskWithOperation::Get() const
{
	int64 Mask = 0;

	switch (Mode)
	{
	case EPCGExBitmaskMode::Composite:
	case EPCGExBitmaskMode::Direct:
		Mask = Bitmask;
		break;
	case EPCGExBitmaskMode::Individual:
		for (const FPCGExClampedBitOp& Bit : Mutations) { Bit.Mutate(Mask); }
		break;
	}

	return Mask;
}

void FPCGExBitmaskWithOperation::DoOperation(int64& Flags) const
{
	if (Mode == EPCGExBitmaskMode::Individual)
	{
		for (const FPCGExClampedBitOp& Bit : Mutations) { Bit.Mutate(Flags); }
		return;
	}

	const int64 Mask = Get();

	switch (Op)
	{
	case EPCGExBitOp::Set:
		Flags = Mask;
		break;
	case EPCGExBitOp::AND:
		Flags &= Mask;
		break;
	case EPCGExBitOp::OR:
		Flags |= Mask;
		break;
	case EPCGExBitOp::NOT:
		Flags &= ~Mask;
		break;
	case EPCGExBitOp::XOR:
		Flags ^= Mask;
		break;
	default: ;
	}
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
