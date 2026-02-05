// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/AutomationTest.h"
#include "Math/PCGExMathBounds.h"

/**
 * Tests for PCGExMathBounds.h
 * Covers: SanitizeBounds, EPCGExBoxCheckMode enum
 */

//////////////////////////////////////////////////////////////////////////
// SanitizeBounds Tests
//////////////////////////////////////////////////////////////////////////

#pragma region SanitizeBounds

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathBoundsSanitizeNormalTest,
	"PCGEx.Unit.Math.MathBounds.SanitizeBounds.NormalBox",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMathBoundsSanitizeNormalTest::RunTest(const FString& Parameters)
{
	// Normal box should remain unchanged
	FBox Box(FVector(0, 0, 0), FVector(10, 10, 10));
	const FVector OriginalMin = Box.Min;
	const FVector OriginalMax = Box.Max;

	PCGExMath::SanitizeBounds(Box);

	TestTrue(TEXT("Normal box Min unchanged"), Box.Min.Equals(OriginalMin));
	TestTrue(TEXT("Normal box Max unchanged"), Box.Max.Equals(OriginalMax));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathBoundsSanitizeZeroSizeTest,
	"PCGEx.Unit.Math.MathBounds.SanitizeBounds.ZeroSize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMathBoundsSanitizeZeroSizeTest::RunTest(const FString& Parameters)
{
	// Box with zero size in all dimensions
	FBox Box(FVector(5, 5, 5), FVector(5, 5, 5));
	const FVector OriginalMin = Box.Min;

	PCGExMath::SanitizeBounds(Box);

	// Min should be modified (decreased by UE_SMALL_NUMBER for each zero-size dimension)
	TestTrue(TEXT("X Min was adjusted"), Box.Min.X < OriginalMin.X);
	TestTrue(TEXT("Y Min was adjusted"), Box.Min.Y < OriginalMin.Y);
	TestTrue(TEXT("Z Min was adjusted"), Box.Min.Z < OriginalMin.Z);

	// Size should now be positive (UE_SMALL_NUMBER)
	const FVector Size = Box.GetSize();
	TestTrue(TEXT("X dimension is positive"), Size.X > 0);
	TestTrue(TEXT("Y dimension is positive"), Size.Y > 0);
	TestTrue(TEXT("Z dimension is positive"), Size.Z > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathBoundsSanitizePartialZeroTest,
	"PCGEx.Unit.Math.MathBounds.SanitizeBounds.PartialZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMathBoundsSanitizePartialZeroTest::RunTest(const FString& Parameters)
{
	// Box with zero size only in X dimension
	FBox Box(FVector(5, 0, 0), FVector(5, 10, 10));
	const double OriginalMinX = Box.Min.X;

	PCGExMath::SanitizeBounds(Box);

	const FVector Size = Box.GetSize();
	TestTrue(TEXT("X Min was adjusted"), Box.Min.X < OriginalMinX);
	TestTrue(TEXT("X dimension is now positive"), Size.X > 0);
	TestTrue(TEXT("Y dimension unchanged"), FMath::IsNearlyEqual(Size.Y, 10.0));
	TestTrue(TEXT("Z dimension unchanged"), FMath::IsNearlyEqual(Size.Z, 10.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathBoundsSanitizeNaNTest,
	"PCGEx.Unit.Math.MathBounds.SanitizeBounds.NaN",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMathBoundsSanitizeNaNTest::RunTest(const FString& Parameters)
{
	// Box with NaN values - size calculation will produce NaN
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	FBox Box(FVector(0, 0, 0), FVector(NaN, 10, 10));

	PCGExMath::SanitizeBounds(Box);

	// Min should be adjusted to create non-zero size
	// The function checks if size is NaN and adjusts Min accordingly
	TestTrue(TEXT("Min.X was adjusted for NaN"), Box.Min.X != 0.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathBoundsSanitizeNearlyZeroTest,
	"PCGEx.Unit.Math.MathBounds.SanitizeBounds.NearlyZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMathBoundsSanitizeNearlyZeroTest::RunTest(const FString& Parameters)
{
	// Box with nearly zero size (smaller than UE_SMALL_NUMBER)
	const double Tiny = UE_SMALL_NUMBER * 0.1;
	FBox Box(FVector(0, 0, 0), FVector(Tiny, 10, 10));

	PCGExMath::SanitizeBounds(Box);

	const FVector Size = Box.GetSize();
	// X should now be non-zero
	TestTrue(TEXT("Nearly-zero X dimension is now non-zero"), !FMath::IsNearlyZero(Size.X));

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// EPCGExBoxCheckMode Enum Tests
//////////////////////////////////////////////////////////////////////////

#pragma region BoxCheckMode

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathBoundsBoxCheckModeEnumTest,
	"PCGEx.Unit.Math.MathBounds.BoxCheckMode.EnumValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMathBoundsBoxCheckModeEnumTest::RunTest(const FString& Parameters)
{
	// Verify enum values
	TestEqual(TEXT("Box = 0"), static_cast<uint8>(EPCGExBoxCheckMode::Box), 0);
	TestEqual(TEXT("ExpandedBox = 1"), static_cast<uint8>(EPCGExBoxCheckMode::ExpandedBox), 1);
	TestEqual(TEXT("Sphere = 2"), static_cast<uint8>(EPCGExBoxCheckMode::Sphere), 2);
	TestEqual(TEXT("ExpandedSphere = 3"), static_cast<uint8>(EPCGExBoxCheckMode::ExpandedSphere), 3);

	return true;
}

#pragma endregion
