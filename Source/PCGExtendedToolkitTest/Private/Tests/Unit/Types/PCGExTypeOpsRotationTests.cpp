// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/AutomationTest.h"
#include "Types/PCGExTypeOpsRotation.h"

/**
 * Tests for PCGExTypeOpsRotation.h
 * Covers: FTypeOps<FRotator>, FTypeOps<FQuat>, FTypeOps<FTransform>
 */

//////////////////////////////////////////////////////////////////////////
// FRotator Type Operations
//////////////////////////////////////////////////////////////////////////

#pragma region FRotator

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsRotatorDefaultTest,
	"PCGEx.Unit.Types.TypeOpsRotation.FRotator.GetDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsRotatorDefaultTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FRotator Default = FTypeOps<FRotator>::GetDefault();
	TestTrue(TEXT("Default rotator should be ZeroRotator"), Default.IsNearlyZero());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsRotatorConversionsTest,
	"PCGEx.Unit.Types.TypeOpsRotation.FRotator.Conversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsRotatorConversionsTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FRotator Rot(45.0, 90.0, 180.0); // Pitch=45, Yaw=90, Roll=180

	// ConvertTo tests
	TestTrue(TEXT("Non-zero rotator converts to true"), FTypeOps<FRotator>::ConvertTo<bool>(Rot));
	TestFalse(TEXT("Zero rotator converts to false"), FTypeOps<FRotator>::ConvertTo<bool>(FRotator::ZeroRotator));

	TestEqual(TEXT("Rotator to int32 uses Pitch"), FTypeOps<FRotator>::ConvertTo<int32>(Rot), 45);
	TestTrue(TEXT("Rotator to float uses Pitch"), FMath::IsNearlyEqual(FTypeOps<FRotator>::ConvertTo<float>(Rot), 45.0f));
	TestTrue(TEXT("Rotator to double uses Pitch"), FMath::IsNearlyEqual(FTypeOps<FRotator>::ConvertTo<double>(Rot), 45.0));

	const FVector2D V2D = FTypeOps<FRotator>::ConvertTo<FVector2D>(Rot);
	TestTrue(TEXT("Rotator to FVector2D X=Pitch"), FMath::IsNearlyEqual(V2D.X, 45.0));
	TestTrue(TEXT("Rotator to FVector2D Y=Yaw"), FMath::IsNearlyEqual(V2D.Y, 90.0));

	const FVector V3D = FTypeOps<FRotator>::ConvertTo<FVector>(Rot);
	TestTrue(TEXT("Rotator to FVector X=Pitch"), FMath::IsNearlyEqual(V3D.X, 45.0));
	TestTrue(TEXT("Rotator to FVector Y=Yaw"), FMath::IsNearlyEqual(V3D.Y, 90.0));
	TestTrue(TEXT("Rotator to FVector Z=Roll"), FMath::IsNearlyEqual(V3D.Z, 180.0));

	const FQuat Quat = FTypeOps<FRotator>::ConvertTo<FQuat>(Rot);
	TestTrue(TEXT("Rotator to FQuat preserves rotation"), Quat.Rotator().Equals(Rot, 0.01));

	// ConvertFrom tests
	const FRotator FromBool = FTypeOps<FRotator>::ConvertFrom(true);
	TestTrue(TEXT("bool true creates 180,180,180"), FMath::IsNearlyEqual(FromBool.Pitch, 180.0));

	const FRotator FromInt = FTypeOps<FRotator>::ConvertFrom(30);
	TestTrue(TEXT("int32 creates uniform rotator"), FMath::IsNearlyEqual(FromInt.Pitch, 30.0));
	TestTrue(TEXT("int32 creates uniform rotator"), FMath::IsNearlyEqual(FromInt.Yaw, 30.0));

	const FRotator FromVec = FTypeOps<FRotator>::ConvertFrom(FVector(10.0, 20.0, 30.0));
	TestTrue(TEXT("FVector to Rotator X=Pitch"), FMath::IsNearlyEqual(FromVec.Pitch, 10.0));
	TestTrue(TEXT("FVector to Rotator Y=Yaw"), FMath::IsNearlyEqual(FromVec.Yaw, 20.0));
	TestTrue(TEXT("FVector to Rotator Z=Roll"), FMath::IsNearlyEqual(FromVec.Roll, 30.0));

	// String round-trip
	const FString RotStr = FTypeOps<FRotator>::ConvertTo<FString>(Rot);
	const FRotator FromStr = FTypeOps<FRotator>::ConvertFrom(RotStr);
	TestTrue(TEXT("String round-trip preserves rotator"), FromStr.Equals(Rot, 0.01));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsRotatorBlendTest,
	"PCGEx.Unit.Types.TypeOpsRotation.FRotator.Blend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsRotatorBlendTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FRotator A(10.0, 20.0, 30.0);
	const FRotator B(40.0, 50.0, 60.0);

	// Add
	const FRotator Sum = FTypeOps<FRotator>::Add(A, B);
	TestTrue(TEXT("Add Pitch"), FMath::IsNearlyEqual(Sum.Pitch, 50.0));
	TestTrue(TEXT("Add Yaw"), FMath::IsNearlyEqual(Sum.Yaw, 70.0));
	TestTrue(TEXT("Add Roll"), FMath::IsNearlyEqual(Sum.Roll, 90.0));

	// Sub
	const FRotator Diff = FTypeOps<FRotator>::Sub(B, A);
	TestTrue(TEXT("Sub Pitch"), FMath::IsNearlyEqual(Diff.Pitch, 30.0));
	TestTrue(TEXT("Sub Yaw"), FMath::IsNearlyEqual(Diff.Yaw, 30.0));
	TestTrue(TEXT("Sub Roll"), FMath::IsNearlyEqual(Diff.Roll, 30.0));

	// Mult (component-wise)
	const FRotator Prod = FTypeOps<FRotator>::Mult(A, B);
	TestTrue(TEXT("Mult Pitch"), FMath::IsNearlyEqual(Prod.Pitch, 400.0));
	TestTrue(TEXT("Mult Yaw"), FMath::IsNearlyEqual(Prod.Yaw, 1000.0));
	TestTrue(TEXT("Mult Roll"), FMath::IsNearlyEqual(Prod.Roll, 1800.0));

	// Div
	const FRotator Quot = FTypeOps<FRotator>::Div(B, 2.0);
	TestTrue(TEXT("Div Pitch"), FMath::IsNearlyEqual(Quot.Pitch, 20.0));
	TestTrue(TEXT("Div Yaw"), FMath::IsNearlyEqual(Quot.Yaw, 25.0));
	TestTrue(TEXT("Div Roll"), FMath::IsNearlyEqual(Quot.Roll, 30.0));

	// Div by zero returns original
	const FRotator QuotZero = FTypeOps<FRotator>::Div(A, 0.0);
	TestTrue(TEXT("Div by zero returns A"), QuotZero.Equals(A));

	// Lerp
	const FRotator Lerped = FTypeOps<FRotator>::Lerp(A, B, 0.5);
	TestTrue(TEXT("Lerp 0.5 Pitch"), FMath::IsNearlyEqual(Lerped.Pitch, 25.0));
	TestTrue(TEXT("Lerp 0.5 Yaw"), FMath::IsNearlyEqual(Lerped.Yaw, 35.0));
	TestTrue(TEXT("Lerp 0.5 Roll"), FMath::IsNearlyEqual(Lerped.Roll, 45.0));

	// Min/Max
	const FRotator Min = FTypeOps<FRotator>::Min(A, B);
	TestTrue(TEXT("Min Pitch"), FMath::IsNearlyEqual(Min.Pitch, 10.0));
	TestTrue(TEXT("Min Yaw"), FMath::IsNearlyEqual(Min.Yaw, 20.0));

	const FRotator Max = FTypeOps<FRotator>::Max(A, B);
	TestTrue(TEXT("Max Pitch"), FMath::IsNearlyEqual(Max.Pitch, 40.0));
	TestTrue(TEXT("Max Yaw"), FMath::IsNearlyEqual(Max.Yaw, 50.0));

	// Average
	const FRotator Avg = FTypeOps<FRotator>::Average(A, B);
	TestTrue(TEXT("Average Pitch"), FMath::IsNearlyEqual(Avg.Pitch, 25.0));

	// Copy
	TestTrue(TEXT("CopyA returns A"), FTypeOps<FRotator>::CopyA(A, B).Equals(A));
	TestTrue(TEXT("CopyB returns B"), FTypeOps<FRotator>::CopyB(A, B).Equals(B));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsRotatorModuloTest,
	"PCGEx.Unit.Types.TypeOpsRotation.FRotator.Modulo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsRotatorModuloTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FRotator A(100.0, 200.0, 300.0);

	// ModSimple
	const FRotator ModS = FTypeOps<FRotator>::ModSimple(A, 90.0);
	TestTrue(TEXT("ModSimple Pitch"), FMath::IsNearlyEqual(ModS.Pitch, 10.0));
	TestTrue(TEXT("ModSimple Yaw"), FMath::IsNearlyEqual(ModS.Yaw, 20.0));
	TestTrue(TEXT("ModSimple Roll"), FMath::IsNearlyEqual(ModS.Roll, 30.0));

	// ModSimple with zero
	const FRotator ModZero = FTypeOps<FRotator>::ModSimple(A, 0.0);
	TestTrue(TEXT("ModSimple zero returns original"), ModZero.Equals(A));

	// ModComplex
	const FRotator B(33.0, 45.0, 60.0);
	const FRotator ModC = FTypeOps<FRotator>::ModComplex(A, B);
	TestTrue(TEXT("ModComplex Pitch"), FMath::IsNearlyEqual(ModC.Pitch, FMath::Fmod(100.0, 33.0)));
	TestTrue(TEXT("ModComplex Yaw"), FMath::IsNearlyEqual(ModC.Yaw, FMath::Fmod(200.0, 45.0)));
	TestTrue(TEXT("ModComplex Roll"), FMath::IsNearlyEqual(ModC.Roll, FMath::Fmod(300.0, 60.0)));

	// ModComplex with zero component
	const FRotator BZero(33.0, 0.0, 60.0);
	const FRotator ModCZ = FTypeOps<FRotator>::ModComplex(A, BZero);
	TestTrue(TEXT("ModComplex zero component preserves original"), FMath::IsNearlyEqual(ModCZ.Yaw, 200.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsRotatorAbsFactorTest,
	"PCGEx.Unit.Types.TypeOpsRotation.FRotator.AbsAndFactor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsRotatorAbsFactorTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FRotator A(-10.0, 20.0, -30.0);

	// Abs
	const FRotator Abs = FTypeOps<FRotator>::Abs(A);
	TestTrue(TEXT("Abs Pitch"), FMath::IsNearlyEqual(Abs.Pitch, 10.0));
	TestTrue(TEXT("Abs Yaw"), FMath::IsNearlyEqual(Abs.Yaw, 20.0));
	TestTrue(TEXT("Abs Roll"), FMath::IsNearlyEqual(Abs.Roll, 30.0));

	// Factor
	const FRotator Factored = FTypeOps<FRotator>::Factor(A, 2.0);
	TestTrue(TEXT("Factor Pitch"), FMath::IsNearlyEqual(Factored.Pitch, -20.0));
	TestTrue(TEXT("Factor Yaw"), FMath::IsNearlyEqual(Factored.Yaw, 40.0));
	TestTrue(TEXT("Factor Roll"), FMath::IsNearlyEqual(Factored.Roll, -60.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsRotatorHashTest,
	"PCGEx.Unit.Types.TypeOpsRotation.FRotator.Hash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsRotatorHashTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FRotator A(10.0, 20.0, 30.0);
	const FRotator B(40.0, 50.0, 60.0);

	const uint32 Hash = FTypeOps<FRotator>::Hash(A);
	TestTrue(TEXT("Hash returns non-zero for non-zero rotator"), Hash != 0);

	const FRotator NaiveH = FTypeOps<FRotator>::NaiveHash(A, B);
	const FRotator UnsignedH = FTypeOps<FRotator>::UnsignedHash(A, B);

	// NaiveHash(A, B) should differ from NaiveHash(B, A)
	const FRotator NaiveHRev = FTypeOps<FRotator>::NaiveHash(B, A);
	// UnsignedHash should be same regardless of order
	const FRotator UnsignedHRev = FTypeOps<FRotator>::UnsignedHash(B, A);

	TestTrue(TEXT("UnsignedHash is order-independent"), UnsignedH.Equals(UnsignedHRev));

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FQuat Type Operations
//////////////////////////////////////////////////////////////////////////

#pragma region FQuat

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsQuatDefaultTest,
	"PCGEx.Unit.Types.TypeOpsRotation.FQuat.GetDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsQuatDefaultTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FQuat Default = FTypeOps<FQuat>::GetDefault();
	TestTrue(TEXT("Default quat should be Identity"), Default.Equals(FQuat::Identity));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsQuatConversionsTest,
	"PCGEx.Unit.Types.TypeOpsRotation.FQuat.Conversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsQuatConversionsTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FQuat Q = FRotator(45.0, 90.0, 0.0).Quaternion();

	// ConvertTo tests
	TestTrue(TEXT("Non-identity quat converts to true"), FTypeOps<FQuat>::ConvertTo<bool>(Q));
	TestFalse(TEXT("Identity quat converts to false"), FTypeOps<FQuat>::ConvertTo<bool>(FQuat::Identity));

	const FRotator Rot = FTypeOps<FQuat>::ConvertTo<FRotator>(Q);
	TestTrue(TEXT("Quat to Rotator preserves rotation"), Rot.Equals(Q.Rotator(), 0.01));

	const FVector4 V4 = FTypeOps<FQuat>::ConvertTo<FVector4>(Q);
	TestTrue(TEXT("Quat to FVector4 X"), FMath::IsNearlyEqual(V4.X, Q.X));
	TestTrue(TEXT("Quat to FVector4 W"), FMath::IsNearlyEqual(V4.W, Q.W));

	// ConvertFrom tests
	const FQuat FromBoolTrue = FTypeOps<FQuat>::ConvertFrom(true);
	const FQuat FromBoolFalse = FTypeOps<FQuat>::ConvertFrom(false);
	// bool true creates FRotator(180,180,180) which due to gimbal lock may equal identity
	// bool false creates FRotator(0,0,0) which is definitely identity
	TestTrue(TEXT("bool false creates identity quat"), FromBoolFalse.Equals(FQuat::Identity, 0.01));
	TestTrue(TEXT("bool conversion produces normalized quat"), FMath::IsNearlyEqual(FromBoolTrue.Size(), 1.0, 0.01));

	const FQuat FromRotator = FTypeOps<FQuat>::ConvertFrom(FRotator(45.0, 90.0, 0.0));
	TestTrue(TEXT("Rotator round-trip"), FromRotator.Equals(Q, 0.01));

	// String round-trip
	const FString QStr = FTypeOps<FQuat>::ConvertTo<FString>(Q);
	const FQuat FromStr = FTypeOps<FQuat>::ConvertFrom(QStr);
	TestTrue(TEXT("String round-trip preserves quat"), FromStr.Equals(Q, 0.01));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsQuatBlendTest,
	"PCGEx.Unit.Types.TypeOpsRotation.FQuat.Blend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsQuatBlendTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FQuat A = FRotator(0.0, 0.0, 0.0).Quaternion();
	const FQuat B = FRotator(90.0, 0.0, 0.0).Quaternion();

	// Lerp (Slerp)
	const FQuat Slerped = FTypeOps<FQuat>::Lerp(A, B, 0.5);
	const FRotator SlerpedRot = Slerped.Rotator();
	TestTrue(TEXT("Slerp 0.5 interpolates rotation"), FMath::IsNearlyEqual(SlerpedRot.Pitch, 45.0, 1.0));

	// Average (Slerp at 0.5)
	const FQuat Avg = FTypeOps<FQuat>::Average(A, B);
	TestTrue(TEXT("Average equals Slerp 0.5"), Avg.Equals(Slerped, 0.01));

	// Mult (quaternion multiplication)
	const FQuat Mult = FTypeOps<FQuat>::Mult(A, B);
	TestTrue(TEXT("Mult produces normalized quat"), FMath::IsNearlyEqual(Mult.Size(), 1.0, 0.01));

	// Min/Max based on angle
	const FQuat SmallAngle = FRotator(10.0, 0.0, 0.0).Quaternion();
	const FQuat LargeAngle = FRotator(80.0, 0.0, 0.0).Quaternion();

	const FQuat Min = FTypeOps<FQuat>::Min(SmallAngle, LargeAngle);
	TestTrue(TEXT("Min returns smaller angle"), Min.Equals(SmallAngle, 0.01));

	const FQuat Max = FTypeOps<FQuat>::Max(SmallAngle, LargeAngle);
	TestTrue(TEXT("Max returns larger angle"), Max.Equals(LargeAngle, 0.01));

	// Copy
	TestTrue(TEXT("CopyA returns A"), FTypeOps<FQuat>::CopyA(A, B).Equals(A));
	TestTrue(TEXT("CopyB returns B"), FTypeOps<FQuat>::CopyB(A, B).Equals(B));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsQuatModuloTest,
	"PCGEx.Unit.Types.TypeOpsRotation.FQuat.Modulo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsQuatModuloTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FQuat A = FRotator(100.0, 200.0, 300.0).Quaternion();

	// ModSimple converts to rotator, mods, converts back
	const FQuat ModS = FTypeOps<FQuat>::ModSimple(A, 90.0);
	const FRotator ModSRot = ModS.Rotator();
	// Result should have components modulo 90
	TestTrue(TEXT("ModSimple produces valid quat"), FMath::IsNearlyEqual(ModS.Size(), 1.0, 0.01));

	// ModSimple with zero
	const FQuat ModZero = FTypeOps<FQuat>::ModSimple(A, 0.0);
	TestTrue(TEXT("ModSimple zero returns original"), ModZero.Equals(A, 0.01));

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FTransform Type Operations
//////////////////////////////////////////////////////////////////////////

#pragma region FTransform

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsTransformDefaultTest,
	"PCGEx.Unit.Types.TypeOpsRotation.FTransform.GetDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsTransformDefaultTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FTransform Default = FTypeOps<FTransform>::GetDefault();
	TestTrue(TEXT("Default transform should be Identity"), Default.Equals(FTransform::Identity));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsTransformConversionsTest,
	"PCGEx.Unit.Types.TypeOpsRotation.FTransform.Conversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsTransformConversionsTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FTransform T(FRotator(45.0, 0.0, 0.0).Quaternion(), FVector(100.0, 200.0, 300.0), FVector(2.0, 2.0, 2.0));

	// ConvertTo tests
	TestTrue(TEXT("Non-identity transform converts to true"), FTypeOps<FTransform>::ConvertTo<bool>(T));
	TestFalse(TEXT("Identity transform converts to false"), FTypeOps<FTransform>::ConvertTo<bool>(FTransform::Identity));

	const FVector Loc = FTypeOps<FTransform>::ConvertTo<FVector>(T);
	TestTrue(TEXT("Transform to FVector returns location"), Loc.Equals(T.GetLocation()));

	const FVector2D Loc2D = FTypeOps<FTransform>::ConvertTo<FVector2D>(T);
	TestTrue(TEXT("Transform to FVector2D X"), FMath::IsNearlyEqual(Loc2D.X, 100.0));
	TestTrue(TEXT("Transform to FVector2D Y"), FMath::IsNearlyEqual(Loc2D.Y, 200.0));

	const FQuat Rot = FTypeOps<FTransform>::ConvertTo<FQuat>(T);
	TestTrue(TEXT("Transform to FQuat returns rotation"), Rot.Equals(T.GetRotation(), 0.01));

	const FRotator Rotator = FTypeOps<FTransform>::ConvertTo<FRotator>(T);
	TestTrue(TEXT("Transform to FRotator returns rotation"), Rotator.Equals(T.Rotator(), 0.01));

	// ConvertFrom tests
	const FTransform FromVec = FTypeOps<FTransform>::ConvertFrom(FVector(50.0, 100.0, 150.0));
	TestTrue(TEXT("FVector to Transform sets location"), FromVec.GetLocation().Equals(FVector(50.0, 100.0, 150.0)));

	const FTransform FromQuat = FTypeOps<FTransform>::ConvertFrom(FRotator(30.0, 60.0, 0.0).Quaternion());
	TestTrue(TEXT("FQuat to Transform sets rotation"), FromQuat.GetRotation().Equals(FRotator(30.0, 60.0, 0.0).Quaternion(), 0.01));

	// String round-trip
	const FString TStr = FTypeOps<FTransform>::ConvertTo<FString>(T);
	const FTransform FromStr = FTypeOps<FTransform>::ConvertFrom(TStr);
	TestTrue(TEXT("String round-trip preserves transform"), FromStr.Equals(T, 0.01));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsTransformBlendTest,
	"PCGEx.Unit.Types.TypeOpsRotation.FTransform.Blend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsTransformBlendTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FTransform A(FQuat::Identity, FVector(100.0, 0.0, 0.0), FVector(1.0, 1.0, 1.0));
	const FTransform B(FRotator(90.0, 0.0, 0.0).Quaternion(), FVector(200.0, 100.0, 50.0), FVector(2.0, 2.0, 2.0));

	// Add
	const FTransform Sum = FTypeOps<FTransform>::Add(A, B);
	TestTrue(TEXT("Add location X"), FMath::IsNearlyEqual(Sum.GetLocation().X, 300.0));
	TestTrue(TEXT("Add location Y"), FMath::IsNearlyEqual(Sum.GetLocation().Y, 100.0));
	TestTrue(TEXT("Add scale"), Sum.GetScale3D().Equals(FVector(3.0, 3.0, 3.0)));

	// Sub
	const FTransform Diff = FTypeOps<FTransform>::Sub(B, A);
	TestTrue(TEXT("Sub location X"), FMath::IsNearlyEqual(Diff.GetLocation().X, 100.0));
	TestTrue(TEXT("Sub scale"), Diff.GetScale3D().Equals(FVector(1.0, 1.0, 1.0)));

	// Div
	const FTransform Quot = FTypeOps<FTransform>::Div(B, 2.0);
	TestTrue(TEXT("Div location X"), FMath::IsNearlyEqual(Quot.GetLocation().X, 100.0));
	TestTrue(TEXT("Div scale"), Quot.GetScale3D().Equals(FVector(1.0, 1.0, 1.0)));

	// Div by zero returns original
	const FTransform QuotZero = FTypeOps<FTransform>::Div(A, 0.0);
	TestTrue(TEXT("Div by zero returns A"), QuotZero.Equals(A));

	// Lerp (Blend)
	const FTransform Lerped = FTypeOps<FTransform>::Lerp(A, B, 0.5);
	TestTrue(TEXT("Lerp 0.5 location X"), FMath::IsNearlyEqual(Lerped.GetLocation().X, 150.0, 1.0));

	// Min/Max
	const FTransform Min = FTypeOps<FTransform>::Min(A, B);
	TestTrue(TEXT("Min location X"), FMath::IsNearlyEqual(Min.GetLocation().X, 100.0));
	TestTrue(TEXT("Min location Y"), FMath::IsNearlyEqual(Min.GetLocation().Y, 0.0));

	const FTransform Max = FTypeOps<FTransform>::Max(A, B);
	TestTrue(TEXT("Max location X"), FMath::IsNearlyEqual(Max.GetLocation().X, 200.0));
	TestTrue(TEXT("Max location Y"), FMath::IsNearlyEqual(Max.GetLocation().Y, 100.0));

	// Average
	const FTransform Avg = FTypeOps<FTransform>::Average(A, B);
	TestTrue(TEXT("Average equals Lerp 0.5"), Avg.GetLocation().Equals(Lerped.GetLocation(), 1.0));

	// Copy
	TestTrue(TEXT("CopyA returns A"), FTypeOps<FTransform>::CopyA(A, B).Equals(A));
	TestTrue(TEXT("CopyB returns B"), FTypeOps<FTransform>::CopyB(A, B).Equals(B));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsTransformModuloTest,
	"PCGEx.Unit.Types.TypeOpsRotation.FTransform.Modulo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsTransformModuloTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FTransform A(FRotator(100.0, 200.0, 300.0).Quaternion(), FVector(150.0, 250.0, 350.0), FVector(2.5, 3.5, 4.5));

	// ModSimple
	const FTransform ModS = FTypeOps<FTransform>::ModSimple(A, 100.0);
	TestTrue(TEXT("ModSimple location X"), FMath::IsNearlyEqual(ModS.GetLocation().X, 50.0));
	TestTrue(TEXT("ModSimple location Y"), FMath::IsNearlyEqual(ModS.GetLocation().Y, 50.0));
	TestTrue(TEXT("ModSimple location Z"), FMath::IsNearlyEqual(ModS.GetLocation().Z, 50.0));
	TestTrue(TEXT("ModSimple scale X"), FMath::IsNearlyEqual(ModS.GetScale3D().X, FMath::Fmod(2.5, 100.0)));

	// ModSimple with zero
	const FTransform ModZero = FTypeOps<FTransform>::ModSimple(A, 0.0);
	TestTrue(TEXT("ModSimple zero returns original"), ModZero.Equals(A));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsTransformAbsFactorTest,
	"PCGEx.Unit.Types.TypeOpsRotation.FTransform.AbsAndFactor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsTransformAbsFactorTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FTransform A(FQuat::Identity, FVector(-100.0, 200.0, -300.0), FVector(-1.0, 2.0, -3.0));

	// Abs
	const FTransform Abs = FTypeOps<FTransform>::Abs(A);
	TestTrue(TEXT("Abs location X"), FMath::IsNearlyEqual(Abs.GetLocation().X, 100.0));
	TestTrue(TEXT("Abs location Y"), FMath::IsNearlyEqual(Abs.GetLocation().Y, 200.0));
	TestTrue(TEXT("Abs location Z"), FMath::IsNearlyEqual(Abs.GetLocation().Z, 300.0));
	TestTrue(TEXT("Abs scale X"), FMath::IsNearlyEqual(Abs.GetScale3D().X, 1.0));
	TestTrue(TEXT("Abs scale Z"), FMath::IsNearlyEqual(Abs.GetScale3D().Z, 3.0));

	// Factor
	const FTransform Factored = FTypeOps<FTransform>::Factor(A, 2.0);
	TestTrue(TEXT("Factor location X"), FMath::IsNearlyEqual(Factored.GetLocation().X, -200.0));
	TestTrue(TEXT("Factor scale Y"), FMath::IsNearlyEqual(Factored.GetScale3D().Y, 4.0));

	return true;
}

#pragma endregion
