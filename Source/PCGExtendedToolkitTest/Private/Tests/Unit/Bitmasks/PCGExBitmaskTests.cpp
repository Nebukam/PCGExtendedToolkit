// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Bitmask Unit Tests
 *
 * Tests bitmask operations in PCGExBitmaskCommon.h and PCGExBitmaskDetails.h:
 * - EPCGExBitOp operations (Set, AND, OR, NOT, XOR)
 * - EPCGExBitflagComparison methods (MatchPartial, MatchFull, MatchStrict, etc.)
 * - GetBitOp conversion from EPCGExBitOp_OR
 * - FPCGExClampedBitOp mutations
 * - FPCGExSimpleBitmask mutations
 *
 * Test naming convention: PCGEx.Unit.Bitmasks.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Data/Bitmasks/PCGExBitmaskCommon.h"
#include "Data/Bitmasks/PCGExBitmaskDetails.h"
#include "Helpers/PCGExTestHelpers.h"

// =============================================================================
// EPCGExBitOp::Set Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBitmaskOpSetTest,
	"PCGEx.Unit.Bitmasks.Op.Set",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBitmaskOpSetTest::RunTest(const FString& Parameters)
{
	// Set operation: Flags = Mask
	int64 Flags = 0b11110000;
	const int64 Mask = 0b00001111;

	PCGExBitmask::Do(EPCGExBitOp::Set, Flags, Mask);
	TestEqual(TEXT("Set replaces flags with mask"), Flags, Mask);

	// Set with zero
	Flags = 0xFFFFFFFF;
	PCGExBitmask::Do(EPCGExBitOp::Set, Flags, 0);
	TestEqual(TEXT("Set to zero clears all bits"), Flags, static_cast<int64>(0));

	// Set with all bits
	Flags = 0;
	PCGExBitmask::Do(EPCGExBitOp::Set, Flags, static_cast<int64>(-1));
	TestEqual(TEXT("Set to -1 sets all bits"), Flags, static_cast<int64>(-1));

	return true;
}

// =============================================================================
// EPCGExBitOp::AND Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBitmaskOpAndTest,
	"PCGEx.Unit.Bitmasks.Op.AND",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBitmaskOpAndTest::RunTest(const FString& Parameters)
{
	// AND operation: Flags &= Mask (keeps only bits that are in both)
	int64 Flags = 0b11111111;
	const int64 Mask = 0b00001111;

	PCGExBitmask::Do(EPCGExBitOp::AND, Flags, Mask);
	TestEqual(TEXT("AND keeps only masked bits"), Flags, static_cast<int64>(0b00001111));

	// AND with zero clears all
	Flags = 0xFFFFFFFF;
	PCGExBitmask::Do(EPCGExBitOp::AND, Flags, 0);
	TestEqual(TEXT("AND with zero clears all"), Flags, static_cast<int64>(0));

	// AND with same value is idempotent
	Flags = 0b10101010;
	PCGExBitmask::Do(EPCGExBitOp::AND, Flags, 0b10101010);
	TestEqual(TEXT("AND with same value unchanged"), Flags, static_cast<int64>(0b10101010));

	// AND with no overlap
	Flags = 0b11110000;
	PCGExBitmask::Do(EPCGExBitOp::AND, Flags, 0b00001111);
	TestEqual(TEXT("AND with no overlap is zero"), Flags, static_cast<int64>(0));

	return true;
}

// =============================================================================
// EPCGExBitOp::OR Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBitmaskOpOrTest,
	"PCGEx.Unit.Bitmasks.Op.OR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBitmaskOpOrTest::RunTest(const FString& Parameters)
{
	// OR operation: Flags |= Mask (adds bits from mask)
	int64 Flags = 0b11110000;
	const int64 Mask = 0b00001111;

	PCGExBitmask::Do(EPCGExBitOp::OR, Flags, Mask);
	TestEqual(TEXT("OR combines bits"), Flags, static_cast<int64>(0b11111111));

	// OR with zero is identity
	Flags = 0b10101010;
	PCGExBitmask::Do(EPCGExBitOp::OR, Flags, 0);
	TestEqual(TEXT("OR with zero unchanged"), Flags, static_cast<int64>(0b10101010));

	// OR with all bits set
	Flags = 0b00001111;
	PCGExBitmask::Do(EPCGExBitOp::OR, Flags, static_cast<int64>(-1));
	TestEqual(TEXT("OR with -1 sets all bits"), Flags, static_cast<int64>(-1));

	// OR is idempotent
	Flags = 0b11110000;
	PCGExBitmask::Do(EPCGExBitOp::OR, Flags, 0b11110000);
	TestEqual(TEXT("OR with same value unchanged"), Flags, static_cast<int64>(0b11110000));

	return true;
}

// =============================================================================
// EPCGExBitOp::NOT Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBitmaskOpNotTest,
	"PCGEx.Unit.Bitmasks.Op.NOT",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBitmaskOpNotTest::RunTest(const FString& Parameters)
{
	// NOT operation: Flags &= ~Mask (clears bits that are in mask)
	int64 Flags = 0b11111111;
	const int64 Mask = 0b00001111;

	PCGExBitmask::Do(EPCGExBitOp::NOT, Flags, Mask);
	TestEqual(TEXT("NOT clears masked bits"), Flags, static_cast<int64>(0b11110000));

	// NOT with zero is identity
	Flags = 0b10101010;
	PCGExBitmask::Do(EPCGExBitOp::NOT, Flags, 0);
	TestEqual(TEXT("NOT with zero unchanged"), Flags, static_cast<int64>(0b10101010));

	// NOT with all bits clears all
	Flags = static_cast<int64>(-1);
	PCGExBitmask::Do(EPCGExBitOp::NOT, Flags, static_cast<int64>(-1));
	TestEqual(TEXT("NOT with -1 clears all"), Flags, static_cast<int64>(0));

	// NOT is not idempotent (second NOT has no effect if bits already cleared)
	Flags = 0b11111111;
	PCGExBitmask::Do(EPCGExBitOp::NOT, Flags, 0b00001111);
	PCGExBitmask::Do(EPCGExBitOp::NOT, Flags, 0b00001111);
	TestEqual(TEXT("Double NOT with same mask"), Flags, static_cast<int64>(0b11110000));

	return true;
}

// =============================================================================
// EPCGExBitOp::XOR Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBitmaskOpXorTest,
	"PCGEx.Unit.Bitmasks.Op.XOR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBitmaskOpXorTest::RunTest(const FString& Parameters)
{
	// XOR operation: Flags ^= Mask (toggles bits)
	int64 Flags = 0b11110000;
	const int64 Mask = 0b11111111;

	PCGExBitmask::Do(EPCGExBitOp::XOR, Flags, Mask);
	TestEqual(TEXT("XOR toggles all bits"), Flags, static_cast<int64>(0b00001111));

	// XOR with zero is identity
	Flags = 0b10101010;
	PCGExBitmask::Do(EPCGExBitOp::XOR, Flags, 0);
	TestEqual(TEXT("XOR with zero unchanged"), Flags, static_cast<int64>(0b10101010));

	// XOR twice returns to original
	Flags = 0b10101010;
	PCGExBitmask::Do(EPCGExBitOp::XOR, Flags, 0b11111111);
	PCGExBitmask::Do(EPCGExBitOp::XOR, Flags, 0b11111111);
	TestEqual(TEXT("Double XOR returns to original"), Flags, static_cast<int64>(0b10101010));

	// XOR with self clears all
	Flags = 0b11110000;
	PCGExBitmask::Do(EPCGExBitOp::XOR, Flags, 0b11110000);
	TestEqual(TEXT("XOR with self is zero"), Flags, static_cast<int64>(0));

	return true;
}

// =============================================================================
// EPCGExBitflagComparison::MatchPartial Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBitmaskCompareMatchPartialTest,
	"PCGEx.Unit.Bitmasks.Compare.MatchPartial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBitmaskCompareMatchPartialTest::RunTest(const FString& Parameters)
{
	// MatchPartial: Value & Mask != 0 (at least some flags in mask are set)
	const int64 Value = 0b10101010;

	// Has some matching bits
	TestTrue(TEXT("Partial match with overlap"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::MatchPartial, Value, 0b00001010));

	// Has all matching bits (still partial match)
	TestTrue(TEXT("Full overlap is also partial match"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::MatchPartial, Value, 0b10101010));

	// Has one matching bit
	TestTrue(TEXT("Single bit match"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::MatchPartial, Value, 0b00000010));

	// No matching bits
	TestFalse(TEXT("No match when no overlap"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::MatchPartial, Value, 0b01010101));

	// Empty mask never matches
	TestFalse(TEXT("Empty mask never matches"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::MatchPartial, Value, 0));

	return true;
}

// =============================================================================
// EPCGExBitflagComparison::MatchFull Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBitmaskCompareMatchFullTest,
	"PCGEx.Unit.Bitmasks.Compare.MatchFull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBitmaskCompareMatchFullTest::RunTest(const FString& Parameters)
{
	// MatchFull: Value & Mask == Mask (all flags in mask are set)
	const int64 Value = 0b11111111;

	// All mask bits present
	TestTrue(TEXT("Full match when all mask bits set"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::MatchFull, Value, 0b00001111));

	// Subset match
	TestTrue(TEXT("Full match with subset mask"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::MatchFull, Value, 0b00000001));

	// Exact match
	TestTrue(TEXT("Full match with exact mask"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::MatchFull, Value, 0b11111111));

	// Missing some mask bits
	const int64 PartialValue = 0b10100000;
	TestFalse(TEXT("No full match when some mask bits missing"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::MatchFull, PartialValue, 0b10101010));

	// Empty mask always matches
	TestTrue(TEXT("Empty mask always full matches"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::MatchFull, PartialValue, 0));

	return true;
}

// =============================================================================
// EPCGExBitflagComparison::MatchStrict Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBitmaskCompareMatchStrictTest,
	"PCGEx.Unit.Bitmasks.Compare.MatchStrict",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBitmaskCompareMatchStrictTest::RunTest(const FString& Parameters)
{
	// MatchStrict: Value == Mask (flags exactly equal mask)
	const int64 Value = 0b10101010;

	// Exact match
	TestTrue(TEXT("Strict match when exactly equal"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::MatchStrict, Value, 0b10101010));

	// Has extra bits
	TestFalse(TEXT("No strict match with extra bits in value"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::MatchStrict, Value, 0b00001010));

	// Missing bits
	TestFalse(TEXT("No strict match when missing bits"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::MatchStrict, Value, 0b11111111));

	// Zero values
	TestTrue(TEXT("Strict match zero == zero"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::MatchStrict, 0LL, 0LL));

	return true;
}

// =============================================================================
// EPCGExBitflagComparison::NoMatchPartial Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBitmaskCompareNoMatchPartialTest,
	"PCGEx.Unit.Bitmasks.Compare.NoMatchPartial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBitmaskCompareNoMatchPartialTest::RunTest(const FString& Parameters)
{
	// NoMatchPartial: Value & Mask == 0 (no flags from mask are set)
	const int64 Value = 0b11110000;

	// No overlap
	TestTrue(TEXT("No match when no bits overlap"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::NoMatchPartial, Value, 0b00001111));

	// Some overlap
	TestFalse(TEXT("Match partial when some bits overlap"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::NoMatchPartial, Value, 0b11110000));

	// Single bit overlap
	TestFalse(TEXT("Match partial with single bit overlap"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::NoMatchPartial, Value, 0b00010000));

	// Empty mask always has no match
	TestTrue(TEXT("Empty mask is no partial match"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::NoMatchPartial, Value, 0));

	return true;
}

// =============================================================================
// EPCGExBitflagComparison::NoMatchFull Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBitmaskCompareNoMatchFullTest,
	"PCGEx.Unit.Bitmasks.Compare.NoMatchFull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBitmaskCompareNoMatchFullTest::RunTest(const FString& Parameters)
{
	// NoMatchFull: Value & Mask != Mask (not all mask bits are set)
	const int64 Value = 0b10100000;
	const int64 FullMask = 0b10101010;

	// Missing some mask bits
	TestTrue(TEXT("No full match when some mask bits missing"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::NoMatchFull, Value, FullMask));

	// Has all mask bits
	const int64 FullValue = 0b11111111;
	TestFalse(TEXT("Has full match when all mask bits present"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::NoMatchFull, FullValue, FullMask));

	// Empty mask always fully matches (so NoMatchFull is false)
	TestFalse(TEXT("Empty mask always has full match"),
		PCGExBitmask::Compare(EPCGExBitflagComparison::NoMatchFull, Value, 0));

	return true;
}

// =============================================================================
// GetBitOp Conversion Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBitmaskGetBitOpTest,
	"PCGEx.Unit.Bitmasks.GetBitOp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBitmaskGetBitOpTest::RunTest(const FString& Parameters)
{
	// Test conversion from EPCGExBitOp_OR to EPCGExBitOp
	// The OR enum has different ordering: OR=0, Set=1, AND=2, NOT=3, XOR=4

	TestEqual(TEXT("OR maps to OR"),
		PCGExBitmask::GetBitOp(EPCGExBitOp_OR::OR), EPCGExBitOp::OR);
	TestEqual(TEXT("Set maps to Set"),
		PCGExBitmask::GetBitOp(EPCGExBitOp_OR::Set), EPCGExBitOp::Set);
	TestEqual(TEXT("AND maps to AND"),
		PCGExBitmask::GetBitOp(EPCGExBitOp_OR::AND), EPCGExBitOp::AND);
	TestEqual(TEXT("NOT maps to NOT"),
		PCGExBitmask::GetBitOp(EPCGExBitOp_OR::NOT), EPCGExBitOp::NOT);
	TestEqual(TEXT("XOR maps to XOR"),
		PCGExBitmask::GetBitOp(EPCGExBitOp_OR::XOR), EPCGExBitOp::XOR);

	return true;
}

// =============================================================================
// FPCGExClampedBitOp::Mutate Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClampedBitOpMutateTest,
	"PCGEx.Unit.Bitmasks.ClampedBitOp.Mutate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClampedBitOpMutateTest::RunTest(const FString& Parameters)
{
	// Test per-bit mutations via FPCGExClampedBitOp

	// OR operation - sets a bit
	{
		FPCGExClampedBitOp BitOp;
		BitOp.BitIndex = 0;
		BitOp.bValue = true;
		BitOp.Op = EPCGExBitOp::OR;

		int64 Flags = 0;
		BitOp.Mutate(Flags);
		TestEqual(TEXT("OR bit 0 = true sets bit 0"), Flags, static_cast<int64>(1));
	}

	// OR with bValue=false (ORing 0 does nothing)
	{
		FPCGExClampedBitOp BitOp;
		BitOp.BitIndex = 5;
		BitOp.bValue = false;
		BitOp.Op = EPCGExBitOp::OR;

		int64 Flags = 0b11111111;
		BitOp.Mutate(Flags);
		TestEqual(TEXT("OR bit 5 = false does nothing"), Flags, static_cast<int64>(0b11111111));
	}

	// NOT operation - clears a bit
	{
		FPCGExClampedBitOp BitOp;
		BitOp.BitIndex = 4;
		BitOp.bValue = true;
		BitOp.Op = EPCGExBitOp::NOT;

		int64 Flags = 0b11111111;
		BitOp.Mutate(Flags);
		TestEqual(TEXT("NOT bit 4 = true clears bit 4"), Flags, static_cast<int64>(0b11101111));
	}

	// XOR operation - toggles a bit
	{
		FPCGExClampedBitOp BitOp;
		BitOp.BitIndex = 3;
		BitOp.bValue = true;
		BitOp.Op = EPCGExBitOp::XOR;

		int64 Flags = 0b11110000;
		BitOp.Mutate(Flags);
		TestEqual(TEXT("XOR bit 3 = true toggles bit 3"), Flags, static_cast<int64>(0b11111000));
	}

	// High bit index (63)
	{
		FPCGExClampedBitOp BitOp;
		BitOp.BitIndex = 63;
		BitOp.bValue = true;
		BitOp.Op = EPCGExBitOp::OR;

		int64 Flags = 0;
		BitOp.Mutate(Flags);
		TestEqual(TEXT("OR bit 63 sets high bit"), Flags, static_cast<int64>(1LL << 63));
	}

	return true;
}

// =============================================================================
// FPCGExSimpleBitmask::Mutate Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSimpleBitmaskMutateTest,
	"PCGEx.Unit.Bitmasks.SimpleBitmask.Mutate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSimpleBitmaskMutateTest::RunTest(const FString& Parameters)
{
	// Test FPCGExSimpleBitmask mutations

	// OR operation
	{
		FPCGExSimpleBitmask Mask;
		Mask.Bitmask = 0b00001111;
		Mask.Op = EPCGExBitOp::OR;

		int64 Flags = 0b11110000;
		Mask.Mutate(Flags);
		TestEqual(TEXT("SimpleBitmask OR"), Flags, static_cast<int64>(0b11111111));
	}

	// AND operation
	{
		FPCGExSimpleBitmask Mask;
		Mask.Bitmask = 0b00001111;
		Mask.Op = EPCGExBitOp::AND;

		int64 Flags = 0b11111111;
		Mask.Mutate(Flags);
		TestEqual(TEXT("SimpleBitmask AND"), Flags, static_cast<int64>(0b00001111));
	}

	// NOT operation
	{
		FPCGExSimpleBitmask Mask;
		Mask.Bitmask = 0b00001111;
		Mask.Op = EPCGExBitOp::NOT;

		int64 Flags = 0b11111111;
		Mask.Mutate(Flags);
		TestEqual(TEXT("SimpleBitmask NOT"), Flags, static_cast<int64>(0b11110000));
	}

	// XOR operation
	{
		FPCGExSimpleBitmask Mask;
		Mask.Bitmask = 0b11110000;
		Mask.Op = EPCGExBitOp::XOR;

		int64 Flags = 0b10101010;
		Mask.Mutate(Flags);
		TestEqual(TEXT("SimpleBitmask XOR"), Flags, static_cast<int64>(0b01011010));
	}

	// Set operation
	{
		FPCGExSimpleBitmask Mask;
		Mask.Bitmask = 0b00110011;
		Mask.Op = EPCGExBitOp::Set;

		int64 Flags = 0b11111111;
		Mask.Mutate(Flags);
		TestEqual(TEXT("SimpleBitmask Set"), Flags, static_cast<int64>(0b00110011));
	}

	return true;
}

// =============================================================================
// Edge Cases
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBitmaskEdgeCasesTest,
	"PCGEx.Unit.Bitmasks.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBitmaskEdgeCasesTest::RunTest(const FString& Parameters)
{
	// Test with all bits set
	{
		int64 AllBits = static_cast<int64>(-1);
		TestTrue(TEXT("All bits strict match with -1"),
			PCGExBitmask::Compare(EPCGExBitflagComparison::MatchStrict, AllBits, static_cast<int64>(-1)));
	}

	// Test with maximum positive value
	{
		int64 MaxPos = INT64_MAX;
		TestTrue(TEXT("MaxPos partial match"),
			PCGExBitmask::Compare(EPCGExBitflagComparison::MatchPartial, MaxPos, 1LL));
	}

	// Test with minimum value (negative)
	{
		int64 MinVal = INT64_MIN;  // Only high bit set
		TestTrue(TEXT("MinVal has high bit set"),
			PCGExBitmask::Compare(EPCGExBitflagComparison::MatchPartial, MinVal, INT64_MIN));
		TestFalse(TEXT("MinVal has no low bits"),
			PCGExBitmask::Compare(EPCGExBitflagComparison::MatchPartial, MinVal, 1LL));
	}

	// Chained operations
	{
		int64 Flags = 0;
		PCGExBitmask::Do(EPCGExBitOp::OR, Flags, 0b11110000);  // Set high nibble
		PCGExBitmask::Do(EPCGExBitOp::OR, Flags, 0b00001111);  // Set low nibble
		PCGExBitmask::Do(EPCGExBitOp::NOT, Flags, 0b00110011); // Clear some bits
		TestEqual(TEXT("Chained operations result"), Flags, static_cast<int64>(0b11001100));
	}

	// Multiple SimpleBitmask mutations
	{
		TArray<FPCGExSimpleBitmask> Compositions;

		FPCGExSimpleBitmask M1;
		M1.Bitmask = 0b11110000;
		M1.Op = EPCGExBitOp::OR;
		Compositions.Add(M1);

		FPCGExSimpleBitmask M2;
		M2.Bitmask = 0b00001111;
		M2.Op = EPCGExBitOp::OR;
		Compositions.Add(M2);

		FPCGExSimpleBitmask M3;
		M3.Bitmask = 0b00110011;
		M3.Op = EPCGExBitOp::NOT;
		Compositions.Add(M3);

		int64 Flags = 0;
		PCGExBitmask::Mutate(Compositions, Flags);
		TestEqual(TEXT("Composition mutations"), Flags, static_cast<int64>(0b11001100));
	}

	return true;
}

// =============================================================================
// Common Flag Patterns Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBitmaskPatternsTest,
	"PCGEx.Unit.Bitmasks.Patterns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBitmaskPatternsTest::RunTest(const FString& Parameters)
{
	// Test common bitmask usage patterns

	// Check if a specific flag is set
	{
		const int64 FLAG_ACTIVE = 1 << 0;
		const int64 FLAG_VISIBLE = 1 << 1;
		const int64 FLAG_SELECTED = 1 << 2;

		int64 EntityFlags = FLAG_ACTIVE | FLAG_VISIBLE;

		TestTrue(TEXT("Entity is active"),
			PCGExBitmask::Compare(EPCGExBitflagComparison::MatchPartial, EntityFlags, FLAG_ACTIVE));
		TestTrue(TEXT("Entity is visible"),
			PCGExBitmask::Compare(EPCGExBitflagComparison::MatchPartial, EntityFlags, FLAG_VISIBLE));
		TestFalse(TEXT("Entity is not selected"),
			PCGExBitmask::Compare(EPCGExBitflagComparison::MatchPartial, EntityFlags, FLAG_SELECTED));
	}

	// Check if all required flags are set
	{
		const int64 REQUIRED_FLAGS = 0b00000111;  // Bits 0, 1, 2 required
		const int64 EntityA = 0b00000111;         // Has all required
		const int64 EntityB = 0b00000101;         // Missing bit 1

		TestTrue(TEXT("EntityA has all required flags"),
			PCGExBitmask::Compare(EPCGExBitflagComparison::MatchFull, EntityA, REQUIRED_FLAGS));
		TestFalse(TEXT("EntityB missing required flags"),
			PCGExBitmask::Compare(EPCGExBitflagComparison::MatchFull, EntityB, REQUIRED_FLAGS));
	}

	// Check if flags exactly match a state
	{
		const int64 STATE_IDLE = 0b00000001;
		const int64 STATE_RUNNING = 0b00000010;

		int64 CurrentState = STATE_IDLE;

		TestTrue(TEXT("State is exactly IDLE"),
			PCGExBitmask::Compare(EPCGExBitflagComparison::MatchStrict, CurrentState, STATE_IDLE));
		TestFalse(TEXT("State is not exactly RUNNING"),
			PCGExBitmask::Compare(EPCGExBitflagComparison::MatchStrict, CurrentState, STATE_RUNNING));
	}

	// Toggle flag
	{
		const int64 FLAG_TOGGLE = 1 << 5;
		int64 Flags = 0b11000000;

		// Toggle on
		PCGExBitmask::Do(EPCGExBitOp::XOR, Flags, FLAG_TOGGLE);
		TestEqual(TEXT("Toggle on"), Flags, static_cast<int64>(0b11100000));

		// Toggle off
		PCGExBitmask::Do(EPCGExBitOp::XOR, Flags, FLAG_TOGGLE);
		TestEqual(TEXT("Toggle off"), Flags, static_cast<int64>(0b11000000));
	}

	return true;
}
