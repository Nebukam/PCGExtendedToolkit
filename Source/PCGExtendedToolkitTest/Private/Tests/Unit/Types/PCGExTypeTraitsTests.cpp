// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/AutomationTest.h"
#include "Types/PCGExTypeTraits.h"

/**
 * Tests for PCGExTypeTraits.h
 * Covers: TTraits<T> for all supported types - Type, TypeId, and feature flags
 */

//////////////////////////////////////////////////////////////////////////
// Numeric Type Traits Tests
//////////////////////////////////////////////////////////////////////////

#pragma region NumericTraits

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeTraitsBoolTest,
	"PCGEx.Unit.Types.TypeTraits.bool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeTraitsBoolTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	// Type classification
	TestEqual(TEXT("bool Type is Boolean"), TTraits<bool>::Type, EPCGMetadataTypes::Boolean);
	TestEqual(TEXT("bool TypeId matches"), TTraits<bool>::TypeId, static_cast<int16>(EPCGMetadataTypes::Boolean));

	// Feature flags
	TestTrue(TEXT("bool is numeric"), TTraits<bool>::bIsNumeric);
	TestFalse(TEXT("bool is not vector"), TTraits<bool>::bIsVector);
	TestFalse(TEXT("bool is not rotation"), TTraits<bool>::bIsRotation);
	TestFalse(TEXT("bool is not string"), TTraits<bool>::bIsString);
	TestFalse(TEXT("bool does not support lerp"), TTraits<bool>::bSupportsLerp);
	TestTrue(TEXT("bool supports min/max"), TTraits<bool>::bSupportsMinMax);
	TestFalse(TEXT("bool does not support arithmetic"), TTraits<bool>::bSupportsArithmetic);

	// Min/Max
	TestEqual(TEXT("bool Min is false"), TTraits<bool>::Min(), false);
	TestEqual(TEXT("bool Max is true"), TTraits<bool>::Max(), true);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeTraitsInt32Test,
	"PCGEx.Unit.Types.TypeTraits.int32",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeTraitsInt32Test::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	TestEqual(TEXT("int32 Type is Integer32"), TTraits<int32>::Type, EPCGMetadataTypes::Integer32);

	TestTrue(TEXT("int32 is numeric"), TTraits<int32>::bIsNumeric);
	TestFalse(TEXT("int32 is not vector"), TTraits<int32>::bIsVector);
	TestTrue(TEXT("int32 supports lerp"), TTraits<int32>::bSupportsLerp);
	TestTrue(TEXT("int32 supports min/max"), TTraits<int32>::bSupportsMinMax);
	TestTrue(TEXT("int32 supports arithmetic"), TTraits<int32>::bSupportsArithmetic);

	TestEqual(TEXT("int32 Min"), TTraits<int32>::Min(), TNumericLimits<int32>::Min());
	TestEqual(TEXT("int32 Max"), TTraits<int32>::Max(), TNumericLimits<int32>::Max());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeTraitsInt64Test,
	"PCGEx.Unit.Types.TypeTraits.int64",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeTraitsInt64Test::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	TestEqual(TEXT("int64 Type is Integer64"), TTraits<int64>::Type, EPCGMetadataTypes::Integer64);

	TestTrue(TEXT("int64 is numeric"), TTraits<int64>::bIsNumeric);
	TestTrue(TEXT("int64 supports lerp"), TTraits<int64>::bSupportsLerp);
	TestTrue(TEXT("int64 supports arithmetic"), TTraits<int64>::bSupportsArithmetic);

	TestEqual(TEXT("int64 Min"), TTraits<int64>::Min(), TNumericLimits<int64>::Min());
	TestEqual(TEXT("int64 Max"), TTraits<int64>::Max(), TNumericLimits<int64>::Max());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeTraitsFloatTest,
	"PCGEx.Unit.Types.TypeTraits.float",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeTraitsFloatTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	TestEqual(TEXT("float Type is Float"), TTraits<float>::Type, EPCGMetadataTypes::Float);

	TestTrue(TEXT("float is numeric"), TTraits<float>::bIsNumeric);
	TestTrue(TEXT("float supports lerp"), TTraits<float>::bSupportsLerp);
	TestTrue(TEXT("float supports min/max"), TTraits<float>::bSupportsMinMax);
	TestTrue(TEXT("float supports arithmetic"), TTraits<float>::bSupportsArithmetic);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeTraitsDoubleTest,
	"PCGEx.Unit.Types.TypeTraits.double",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeTraitsDoubleTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	TestEqual(TEXT("double Type is Double"), TTraits<double>::Type, EPCGMetadataTypes::Double);

	TestTrue(TEXT("double is numeric"), TTraits<double>::bIsNumeric);
	TestTrue(TEXT("double supports lerp"), TTraits<double>::bSupportsLerp);
	TestTrue(TEXT("double supports arithmetic"), TTraits<double>::bSupportsArithmetic);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Vector Type Traits Tests
//////////////////////////////////////////////////////////////////////////

#pragma region VectorTraits

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeTraitsVector2DTest,
	"PCGEx.Unit.Types.TypeTraits.FVector2D",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeTraitsVector2DTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	TestEqual(TEXT("FVector2D Type is Vector2"), TTraits<FVector2D>::Type, EPCGMetadataTypes::Vector2);

	TestFalse(TEXT("FVector2D is not numeric"), TTraits<FVector2D>::bIsNumeric);
	TestTrue(TEXT("FVector2D is vector"), TTraits<FVector2D>::bIsVector);
	TestFalse(TEXT("FVector2D is not rotation"), TTraits<FVector2D>::bIsRotation);
	TestFalse(TEXT("FVector2D is not string"), TTraits<FVector2D>::bIsString);
	TestTrue(TEXT("FVector2D supports lerp"), TTraits<FVector2D>::bSupportsLerp);
	TestTrue(TEXT("FVector2D supports min/max"), TTraits<FVector2D>::bSupportsMinMax);
	TestTrue(TEXT("FVector2D supports arithmetic"), TTraits<FVector2D>::bSupportsArithmetic);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeTraitsVectorTest,
	"PCGEx.Unit.Types.TypeTraits.FVector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeTraitsVectorTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	TestEqual(TEXT("FVector Type is Vector"), TTraits<FVector>::Type, EPCGMetadataTypes::Vector);

	TestTrue(TEXT("FVector is vector"), TTraits<FVector>::bIsVector);
	TestTrue(TEXT("FVector supports lerp"), TTraits<FVector>::bSupportsLerp);
	TestTrue(TEXT("FVector supports arithmetic"), TTraits<FVector>::bSupportsArithmetic);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeTraitsVector4Test,
	"PCGEx.Unit.Types.TypeTraits.FVector4",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeTraitsVector4Test::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	TestEqual(TEXT("FVector4 Type is Vector4"), TTraits<FVector4>::Type, EPCGMetadataTypes::Vector4);

	TestTrue(TEXT("FVector4 is vector"), TTraits<FVector4>::bIsVector);
	TestTrue(TEXT("FVector4 supports lerp"), TTraits<FVector4>::bSupportsLerp);
	TestTrue(TEXT("FVector4 supports arithmetic"), TTraits<FVector4>::bSupportsArithmetic);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Rotation Type Traits Tests
//////////////////////////////////////////////////////////////////////////

#pragma region RotationTraits

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeTraitsRotatorTest,
	"PCGEx.Unit.Types.TypeTraits.FRotator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeTraitsRotatorTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	TestEqual(TEXT("FRotator Type is Rotator"), TTraits<FRotator>::Type, EPCGMetadataTypes::Rotator);

	TestFalse(TEXT("FRotator is not numeric"), TTraits<FRotator>::bIsNumeric);
	TestFalse(TEXT("FRotator is not vector"), TTraits<FRotator>::bIsVector);
	TestTrue(TEXT("FRotator is rotation"), TTraits<FRotator>::bIsRotation);
	TestFalse(TEXT("FRotator is not string"), TTraits<FRotator>::bIsString);
	TestTrue(TEXT("FRotator supports lerp"), TTraits<FRotator>::bSupportsLerp);
	TestTrue(TEXT("FRotator supports min/max"), TTraits<FRotator>::bSupportsMinMax);
	TestTrue(TEXT("FRotator supports arithmetic"), TTraits<FRotator>::bSupportsArithmetic);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeTraitsQuatTest,
	"PCGEx.Unit.Types.TypeTraits.FQuat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeTraitsQuatTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	TestEqual(TEXT("FQuat Type is Quaternion"), TTraits<FQuat>::Type, EPCGMetadataTypes::Quaternion);

	TestTrue(TEXT("FQuat is rotation"), TTraits<FQuat>::bIsRotation);
	TestTrue(TEXT("FQuat supports lerp"), TTraits<FQuat>::bSupportsLerp);
	TestFalse(TEXT("FQuat does not support min/max"), TTraits<FQuat>::bSupportsMinMax);
	TestFalse(TEXT("FQuat does not support arithmetic"), TTraits<FQuat>::bSupportsArithmetic);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeTraitsTransformTest,
	"PCGEx.Unit.Types.TypeTraits.FTransform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeTraitsTransformTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	TestEqual(TEXT("FTransform Type is Transform"), TTraits<FTransform>::Type, EPCGMetadataTypes::Transform);

	TestFalse(TEXT("FTransform is not numeric"), TTraits<FTransform>::bIsNumeric);
	TestFalse(TEXT("FTransform is not vector"), TTraits<FTransform>::bIsVector);
	TestFalse(TEXT("FTransform is not rotation (composite)"), TTraits<FTransform>::bIsRotation);
	TestFalse(TEXT("FTransform is not string"), TTraits<FTransform>::bIsString);
	TestTrue(TEXT("FTransform supports lerp"), TTraits<FTransform>::bSupportsLerp);
	TestFalse(TEXT("FTransform does not support min/max"), TTraits<FTransform>::bSupportsMinMax);
	TestFalse(TEXT("FTransform does not support arithmetic"), TTraits<FTransform>::bSupportsArithmetic);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// String Type Traits Tests
//////////////////////////////////////////////////////////////////////////

#pragma region StringTraits

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeTraitsFStringTest,
	"PCGEx.Unit.Types.TypeTraits.FString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeTraitsFStringTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	TestEqual(TEXT("FString Type is String"), TTraits<FString>::Type, EPCGMetadataTypes::String);

	TestFalse(TEXT("FString is not numeric"), TTraits<FString>::bIsNumeric);
	TestFalse(TEXT("FString is not vector"), TTraits<FString>::bIsVector);
	TestFalse(TEXT("FString is not rotation"), TTraits<FString>::bIsRotation);
	TestTrue(TEXT("FString is string"), TTraits<FString>::bIsString);
	TestFalse(TEXT("FString does not support lerp"), TTraits<FString>::bSupportsLerp);
	TestFalse(TEXT("FString does not support min/max"), TTraits<FString>::bSupportsMinMax);
	TestFalse(TEXT("FString does not support arithmetic"), TTraits<FString>::bSupportsArithmetic);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeTraitsFNameTest,
	"PCGEx.Unit.Types.TypeTraits.FName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeTraitsFNameTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	TestEqual(TEXT("FName Type is Name"), TTraits<FName>::Type, EPCGMetadataTypes::Name);

	TestTrue(TEXT("FName is string"), TTraits<FName>::bIsString);
	TestFalse(TEXT("FName does not support lerp"), TTraits<FName>::bSupportsLerp);

	TestEqual(TEXT("FName Min is NAME_None"), TTraits<FName>::Min(), NAME_None);
	TestEqual(TEXT("FName Max is NAME_None"), TTraits<FName>::Max(), NAME_None);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeTraitsSoftObjectPathTest,
	"PCGEx.Unit.Types.TypeTraits.FSoftObjectPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeTraitsSoftObjectPathTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	TestEqual(TEXT("FSoftObjectPath Type is SoftObjectPath"), TTraits<FSoftObjectPath>::Type, EPCGMetadataTypes::SoftObjectPath);

	TestTrue(TEXT("FSoftObjectPath is string"), TTraits<FSoftObjectPath>::bIsString);
	TestFalse(TEXT("FSoftObjectPath does not support lerp"), TTraits<FSoftObjectPath>::bSupportsLerp);
	TestFalse(TEXT("FSoftObjectPath does not support min/max"), TTraits<FSoftObjectPath>::bSupportsMinMax);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeTraitsSoftClassPathTest,
	"PCGEx.Unit.Types.TypeTraits.FSoftClassPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeTraitsSoftClassPathTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	TestEqual(TEXT("FSoftClassPath Type is SoftClassPath"), TTraits<FSoftClassPath>::Type, EPCGMetadataTypes::SoftClassPath);

	TestTrue(TEXT("FSoftClassPath is string"), TTraits<FSoftClassPath>::bIsString);
	TestFalse(TEXT("FSoftClassPath does not support lerp"), TTraits<FSoftClassPath>::bSupportsLerp);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Unknown Type Traits Test
//////////////////////////////////////////////////////////////////////////

#pragma region UnknownTraits

// Test that an unsupported type defaults to Unknown
struct FUnknownTestType {};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeTraitsUnknownTest,
	"PCGEx.Unit.Types.TypeTraits.Unknown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeTraitsUnknownTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	// Unspecialized type should have Unknown traits
	TestEqual(TEXT("Unknown Type is Unknown"), TTraits<FUnknownTestType>::Type, EPCGMetadataTypes::Unknown);

	TestFalse(TEXT("Unknown is not numeric"), TTraits<FUnknownTestType>::bIsNumeric);
	TestFalse(TEXT("Unknown is not vector"), TTraits<FUnknownTestType>::bIsVector);
	TestFalse(TEXT("Unknown is not rotation"), TTraits<FUnknownTestType>::bIsRotation);
	TestFalse(TEXT("Unknown is not string"), TTraits<FUnknownTestType>::bIsString);
	TestFalse(TEXT("Unknown does not support lerp"), TTraits<FUnknownTestType>::bSupportsLerp);
	TestFalse(TEXT("Unknown does not support min/max"), TTraits<FUnknownTestType>::bSupportsMinMax);
	TestFalse(TEXT("Unknown does not support arithmetic"), TTraits<FUnknownTestType>::bSupportsArithmetic);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// TypesAllocations Constant Test
//////////////////////////////////////////////////////////////////////////

#pragma region Constants

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeTraitsConstantsTest,
	"PCGEx.Unit.Types.TypeTraits.Constants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeTraitsConstantsTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	// Verify the TypesAllocations constant
	TestEqual(TEXT("TypesAllocations is 15"), TypesAllocations, 15);

	return true;
}

#pragma endregion
