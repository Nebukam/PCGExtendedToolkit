// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/AutomationTest.h"
#include "Types/PCGExTypes.h"

//////////////////////////////////////////////////////////////////
// FScopedTypedValue Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypesFScopedTypedValueNumericTypes,
	"PCGEx.Unit.Types.FScopedTypedValue.NumericTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypesFScopedTypedValueNumericTypes::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	// Test int32
	{
		FScopedTypedValue IntValue(EPCGMetadataTypes::Integer32);
		TestTrue(TEXT("Int32 is constructed"), IntValue.IsConstructed());
		TestEqual(TEXT("Int32 type matches"), IntValue.GetType(), EPCGMetadataTypes::Integer32);

		IntValue.As<int32>() = 42;
		TestEqual(TEXT("Int32 value can be set and read"), IntValue.As<int32>(), 42);
	}

	// Test float
	{
		FScopedTypedValue FloatValue(EPCGMetadataTypes::Float);
		TestTrue(TEXT("Float is constructed"), FloatValue.IsConstructed());
		FloatValue.As<float>() = 3.14f;
		TestTrue(TEXT("Float value matches"), FMath::IsNearlyEqual(FloatValue.As<float>(), 3.14f, 0.001f));
	}

	// Test double
	{
		FScopedTypedValue DoubleValue(EPCGMetadataTypes::Double);
		TestTrue(TEXT("Double is constructed"), DoubleValue.IsConstructed());
		DoubleValue.As<double>() = 3.14159265358979;
		TestTrue(TEXT("Double value matches"), FMath::IsNearlyEqual(DoubleValue.As<double>(), 3.14159265358979, 0.0000001));
	}

	// Test bool
	{
		FScopedTypedValue BoolValue(EPCGMetadataTypes::Boolean);
		TestTrue(TEXT("Bool is constructed"), BoolValue.IsConstructed());
		BoolValue.As<bool>() = true;
		TestTrue(TEXT("Bool value is true"), BoolValue.As<bool>());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypesFScopedTypedValueVectorTypes,
	"PCGEx.Unit.Types.FScopedTypedValue.VectorTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypesFScopedTypedValueVectorTypes::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	// Test FVector2D
	{
		FScopedTypedValue Vec2Value(EPCGMetadataTypes::Vector2);
		TestTrue(TEXT("Vector2D is constructed"), Vec2Value.IsConstructed());
		Vec2Value.As<FVector2D>() = FVector2D(1.0, 2.0);
		TestTrue(TEXT("Vector2D value matches"), Vec2Value.As<FVector2D>().Equals(FVector2D(1.0, 2.0), 0.01f));
	}

	// Test FVector
	{
		FScopedTypedValue VecValue(EPCGMetadataTypes::Vector);
		TestTrue(TEXT("Vector is constructed"), VecValue.IsConstructed());
		VecValue.As<FVector>() = FVector(1.0, 2.0, 3.0);
		TestTrue(TEXT("Vector value matches"), VecValue.As<FVector>().Equals(FVector(1.0, 2.0, 3.0), 0.01f));
	}

	// Test FVector4
	{
		FScopedTypedValue Vec4Value(EPCGMetadataTypes::Vector4);
		TestTrue(TEXT("Vector4 is constructed"), Vec4Value.IsConstructed());
		Vec4Value.As<FVector4>() = FVector4(1.0, 2.0, 3.0, 4.0);
		const FVector4& Result = Vec4Value.As<FVector4>();
		TestTrue(TEXT("Vector4 X matches"), FMath::IsNearlyEqual(Result.X, 1.0, 0.01));
		TestTrue(TEXT("Vector4 W matches"), FMath::IsNearlyEqual(Result.W, 4.0, 0.01));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypesFScopedTypedValueRotationTypes,
	"PCGEx.Unit.Types.FScopedTypedValue.RotationTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypesFScopedTypedValueRotationTypes::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	// Test FRotator
	{
		FScopedTypedValue RotValue(EPCGMetadataTypes::Rotator);
		TestTrue(TEXT("Rotator is constructed"), RotValue.IsConstructed());
		RotValue.As<FRotator>() = FRotator(45.0f, 90.0f, 0.0f);
		TestTrue(TEXT("Rotator value matches"), RotValue.As<FRotator>().Equals(FRotator(45.0f, 90.0f, 0.0f), 0.01f));
	}

	// Test FQuat
	{
		FScopedTypedValue QuatValue(EPCGMetadataTypes::Quaternion);
		TestTrue(TEXT("Quat is constructed"), QuatValue.IsConstructed());
		QuatValue.As<FQuat>() = FQuat::Identity;
		TestTrue(TEXT("Quat is identity"), QuatValue.As<FQuat>().Equals(FQuat::Identity, 0.01f));
	}

	// Test FTransform
	{
		FScopedTypedValue TransformValue(EPCGMetadataTypes::Transform);
		TestTrue(TEXT("Transform is constructed"), TransformValue.IsConstructed());
		const FTransform TestTransform(FQuat::Identity, FVector(100, 200, 300), FVector::OneVector);
		TransformValue.As<FTransform>() = TestTransform;
		TestTrue(TEXT("Transform translation matches"), TransformValue.As<FTransform>().GetTranslation().Equals(FVector(100, 200, 300), 0.01f));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypesFScopedTypedValueStringTypes,
	"PCGEx.Unit.Types.FScopedTypedValue.StringTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypesFScopedTypedValueStringTypes::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	// Test FString (complex type requiring lifecycle management)
	{
		FScopedTypedValue StrValue(EPCGMetadataTypes::String);
		TestTrue(TEXT("String is constructed"), StrValue.IsConstructed());
		StrValue.As<FString>() = TEXT("Hello World");
		TestEqual(TEXT("String value matches"), StrValue.As<FString>(), FString(TEXT("Hello World")));
	}

	// Test FName
	{
		FScopedTypedValue NameValue(EPCGMetadataTypes::Name);
		TestTrue(TEXT("Name is constructed"), NameValue.IsConstructed());
		NameValue.As<FName>() = FName(TEXT("TestName"));
		TestEqual(TEXT("Name value matches"), NameValue.As<FName>(), FName(TEXT("TestName")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypesFScopedTypedValueLifecycle,
	"PCGEx.Unit.Types.FScopedTypedValue.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypesFScopedTypedValueLifecycle::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	// Test Destruct and Initialize for reuse
	{
		FScopedTypedValue Value(EPCGMetadataTypes::Integer32);
		Value.As<int32>() = 42;
		TestEqual(TEXT("Initial int32 value"), Value.As<int32>(), 42);

		Value.Destruct();
		TestFalse(TEXT("Value is destructed"), Value.IsConstructed());

		Value.Initialize(EPCGMetadataTypes::Double);
		TestTrue(TEXT("Value is re-initialized"), Value.IsConstructed());
		TestEqual(TEXT("Type is now double"), Value.GetType(), EPCGMetadataTypes::Double);
		Value.As<double>() = 99.9;
		TestTrue(TEXT("Double value set"), FMath::IsNearlyEqual(Value.As<double>(), 99.9, 0.01));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypesFScopedTypedValueMoveConstructor,
	"PCGEx.Unit.Types.FScopedTypedValue.MoveConstructor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypesFScopedTypedValueMoveConstructor::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	FScopedTypedValue Original(EPCGMetadataTypes::Vector);
	Original.As<FVector>() = FVector(1, 2, 3);

	FScopedTypedValue Moved(MoveTemp(Original));

	TestTrue(TEXT("Moved value is constructed"), Moved.IsConstructed());
	TestTrue(TEXT("Moved value has correct data"), Moved.As<FVector>().Equals(FVector(1, 2, 3), 0.01f));
	TestFalse(TEXT("Original is no longer constructed"), Original.IsConstructed());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypesFScopedTypedValueStaticHelpers,
	"PCGEx.Unit.Types.FScopedTypedValue.StaticHelpers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypesFScopedTypedValueStaticHelpers::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	// POD types don't need lifecycle management
	TestFalse(TEXT("Int32 doesn't need lifecycle"), FScopedTypedValue::NeedsLifecycleManagement(EPCGMetadataTypes::Integer32));
	TestFalse(TEXT("Float doesn't need lifecycle"), FScopedTypedValue::NeedsLifecycleManagement(EPCGMetadataTypes::Float));
	TestFalse(TEXT("Double doesn't need lifecycle"), FScopedTypedValue::NeedsLifecycleManagement(EPCGMetadataTypes::Double));
	TestFalse(TEXT("Bool doesn't need lifecycle"), FScopedTypedValue::NeedsLifecycleManagement(EPCGMetadataTypes::Boolean));
	TestFalse(TEXT("Vector doesn't need lifecycle"), FScopedTypedValue::NeedsLifecycleManagement(EPCGMetadataTypes::Vector));

	// Complex types need lifecycle management
	TestTrue(TEXT("String needs lifecycle"), FScopedTypedValue::NeedsLifecycleManagement(EPCGMetadataTypes::String));
	TestTrue(TEXT("Name needs lifecycle"), FScopedTypedValue::NeedsLifecycleManagement(EPCGMetadataTypes::Name));

	// Type sizes
	TestEqual(TEXT("Int32 size"), FScopedTypedValue::GetTypeSize(EPCGMetadataTypes::Integer32), static_cast<int32>(sizeof(int32)));
	TestEqual(TEXT("Double size"), FScopedTypedValue::GetTypeSize(EPCGMetadataTypes::Double), static_cast<int32>(sizeof(double)));
	TestEqual(TEXT("Vector size"), FScopedTypedValue::GetTypeSize(EPCGMetadataTypes::Vector), static_cast<int32>(sizeof(FVector)));
	TestEqual(TEXT("Transform size"), FScopedTypedValue::GetTypeSize(EPCGMetadataTypes::Transform), static_cast<int32>(sizeof(FTransform)));

	return true;
}

//////////////////////////////////////////////////////////////////
// Convenience Function Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypesConvertFunction,
	"PCGEx.Unit.Types.ConvenienceFunctions.Convert",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypesConvertFunction::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	// Int to double
	const double IntToDouble = Convert<int32, double>(42);
	TestTrue(TEXT("Int to double"), FMath::IsNearlyEqual(IntToDouble, 42.0, 0.001));

	// Float to int
	const int32 FloatToInt = Convert<float, int32>(3.7f);
	TestEqual(TEXT("Float to int truncates"), FloatToInt, 3);

	// Double to float
	const float DoubleToFloat = Convert<double, float>(3.14159);
	TestTrue(TEXT("Double to float"), FMath::IsNearlyEqual(DoubleToFloat, 3.14159f, 0.0001f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypesComputeHashFunction,
	"PCGEx.Unit.Types.ConvenienceFunctions.ComputeHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypesComputeHashFunction::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	// Same values should produce same hash
	const uint32 Hash1 = ComputeHash<int32>(42);
	const uint32 Hash2 = ComputeHash<int32>(42);
	TestEqual(TEXT("Same int values have same hash"), Hash1, Hash2);

	// Different values should (usually) produce different hash
	const uint32 Hash3 = ComputeHash<int32>(43);
	TestNotEqual(TEXT("Different int values have different hash"), Hash1, Hash3);

	// Vector hash
	const uint32 VecHash1 = ComputeHash<FVector>(FVector(1, 2, 3));
	const uint32 VecHash2 = ComputeHash<FVector>(FVector(1, 2, 3));
	TestEqual(TEXT("Same vector values have same hash"), VecHash1, VecHash2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypesLerpFunction,
	"PCGEx.Unit.Types.ConvenienceFunctions.Lerp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypesLerpFunction::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	// Float lerp
	const float FloatResult = Lerp<float>(0.0f, 10.0f, 0.5);
	TestTrue(TEXT("Float lerp at 0.5"), FMath::IsNearlyEqual(FloatResult, 5.0f, 0.001f));

	// Double lerp
	const double DoubleResult = Lerp<double>(0.0, 100.0, 0.25);
	TestTrue(TEXT("Double lerp at 0.25"), FMath::IsNearlyEqual(DoubleResult, 25.0, 0.001));

	// Vector lerp
	const FVector VecResult = Lerp<FVector>(FVector::ZeroVector, FVector(10, 20, 30), 0.5);
	TestTrue(TEXT("Vector lerp at 0.5"), VecResult.Equals(FVector(5, 10, 15), 0.01f));

	// Lerp at boundaries
	TestTrue(TEXT("Lerp at 0"), FMath::IsNearlyEqual(Lerp<float>(10.0f, 20.0f, 0.0), 10.0f, 0.001f));
	TestTrue(TEXT("Lerp at 1"), FMath::IsNearlyEqual(Lerp<float>(10.0f, 20.0f, 1.0), 20.0f, 0.001f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypesClampFunction,
	"PCGEx.Unit.Types.ConvenienceFunctions.Clamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypesClampFunction::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	// Value below min
	TestEqual(TEXT("Clamp below min"), Clamp<int32>(5, 10, 20), 10);

	// Value above max
	TestEqual(TEXT("Clamp above max"), Clamp<int32>(25, 10, 20), 20);

	// Value in range
	TestEqual(TEXT("Clamp in range"), Clamp<int32>(15, 10, 20), 15);

	// Float clamp
	TestTrue(TEXT("Float clamp below"), FMath::IsNearlyEqual(Clamp<float>(0.5f, 1.0f, 2.0f), 1.0f, 0.001f));
	TestTrue(TEXT("Float clamp above"), FMath::IsNearlyEqual(Clamp<float>(2.5f, 1.0f, 2.0f), 2.0f, 0.001f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypesAbsFunction,
	"PCGEx.Unit.Types.ConvenienceFunctions.Abs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypesAbsFunction::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	// Int abs
	TestEqual(TEXT("Abs of negative int"), Abs<int32>(-42), 42);
	TestEqual(TEXT("Abs of positive int"), Abs<int32>(42), 42);
	TestEqual(TEXT("Abs of zero"), Abs<int32>(0), 0);

	// Float abs
	TestTrue(TEXT("Abs of negative float"), FMath::IsNearlyEqual(Abs<float>(-3.14f), 3.14f, 0.001f));

	// Vector abs (component-wise)
	const FVector AbsVec = Abs<FVector>(FVector(-1, -2, 3));
	TestTrue(TEXT("Vector abs"), AbsVec.Equals(FVector(1, 2, 3), 0.01f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypesFactorFunction,
	"PCGEx.Unit.Types.ConvenienceFunctions.Factor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypesFactorFunction::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	// Float factor
	const float FloatResult = Factor<float>(10.0f, 0.5);
	TestTrue(TEXT("Float factor by 0.5"), FMath::IsNearlyEqual(FloatResult, 5.0f, 0.001f));

	// Double factor
	const double DoubleResult = Factor<double>(100.0, 2.0);
	TestTrue(TEXT("Double factor by 2.0"), FMath::IsNearlyEqual(DoubleResult, 200.0, 0.001));

	// Vector factor
	const FVector VecResult = Factor<FVector>(FVector(10, 20, 30), 0.5);
	TestTrue(TEXT("Vector factor by 0.5"), VecResult.Equals(FVector(5, 10, 15), 0.01f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypesAreEqualFunction,
	"PCGEx.Unit.Types.ConvenienceFunctions.AreEqual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypesAreEqualFunction::RunTest(const FString& Parameters)
{
	using namespace PCGExTypes;

	// Int equality
	TestTrue(TEXT("Equal ints"), AreEqual<int32>(42, 42));
	TestFalse(TEXT("Unequal ints"), AreEqual<int32>(42, 43));

	// Float equality (note: uses exact comparison)
	TestTrue(TEXT("Equal floats"), AreEqual<float>(3.14f, 3.14f));
	TestFalse(TEXT("Unequal floats"), AreEqual<float>(3.14f, 3.15f));

	// String equality
	TestTrue(TEXT("Equal strings"), AreEqual<FString>(FString(TEXT("Test")), FString(TEXT("Test"))));
	TestFalse(TEXT("Unequal strings"), AreEqual<FString>(FString(TEXT("Test")), FString(TEXT("Other"))));

	return true;
}
