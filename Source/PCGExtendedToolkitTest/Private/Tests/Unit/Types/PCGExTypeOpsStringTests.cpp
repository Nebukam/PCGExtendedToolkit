// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/AutomationTest.h"
#include "Types/PCGExTypeOpsString.h"

/**
 * Tests for PCGExTypeOpsString.h
 * Covers: FTypeOps<FString>, FTypeOps<FName>, FTypeOps<FSoftObjectPath>, FTypeOps<FSoftClassPath>
 */

//////////////////////////////////////////////////////////////////////////
// FString Type Operations
//////////////////////////////////////////////////////////////////////////

#pragma region FString

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsStringDefaultTest,
	"PCGEx.Unit.Types.TypeOpsString.FString.GetDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsStringDefaultTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FString Default = FTypeOps<FString>::GetDefault();
	TestTrue(TEXT("Default string should be empty"), Default.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsStringConversionsTest,
	"PCGEx.Unit.Types.TypeOpsString.FString.Conversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsStringConversionsTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	// ConvertTo bool
	TestTrue(TEXT("'true' converts to true"), FTypeOps<FString>::ConvertTo<bool>(TEXT("true")));
	TestTrue(TEXT("'True' converts to true"), FTypeOps<FString>::ConvertTo<bool>(TEXT("True")));
	TestFalse(TEXT("'false' converts to false"), FTypeOps<FString>::ConvertTo<bool>(TEXT("false")));
	TestFalse(TEXT("Empty string converts to false"), FTypeOps<FString>::ConvertTo<bool>(TEXT("")));

	// ConvertTo numeric
	TestEqual(TEXT("'42' to int32"), FTypeOps<FString>::ConvertTo<int32>(TEXT("42")), 42);
	TestEqual(TEXT("'-100' to int32"), FTypeOps<FString>::ConvertTo<int32>(TEXT("-100")), -100);
	TestTrue(TEXT("'3.14' to float"), FMath::IsNearlyEqual(FTypeOps<FString>::ConvertTo<float>(TEXT("3.14")), 3.14f, 0.01f));
	TestTrue(TEXT("'2.718' to double"), FMath::IsNearlyEqual(FTypeOps<FString>::ConvertTo<double>(TEXT("2.718")), 2.718, 0.001));

	// ConvertTo FVector (via InitFromString)
	const FString VecStr = TEXT("X=1.0 Y=2.0 Z=3.0");
	const FVector Vec = FTypeOps<FString>::ConvertTo<FVector>(VecStr);
	TestTrue(TEXT("String to FVector X"), FMath::IsNearlyEqual(Vec.X, 1.0));
	TestTrue(TEXT("String to FVector Y"), FMath::IsNearlyEqual(Vec.Y, 2.0));
	TestTrue(TEXT("String to FVector Z"), FMath::IsNearlyEqual(Vec.Z, 3.0));

	// ConvertTo FName
	const FName Name = FTypeOps<FString>::ConvertTo<FName>(TEXT("TestName"));
	TestEqual(TEXT("String to FName"), Name, FName(TEXT("TestName")));

	// ConvertTo FSoftObjectPath
	const FSoftObjectPath Path = FTypeOps<FString>::ConvertTo<FSoftObjectPath>(TEXT("/Game/Test/Asset"));
	TestEqual(TEXT("String to FSoftObjectPath"), Path.ToString(), TEXT("/Game/Test/Asset"));

	// ConvertFrom bool
	TestEqual(TEXT("true to string"), FTypeOps<FString>::ConvertFrom(true), TEXT("true"));
	TestEqual(TEXT("false to string"), FTypeOps<FString>::ConvertFrom(false), TEXT("false"));

	// ConvertFrom numeric
	TestEqual(TEXT("int32 to string"), FTypeOps<FString>::ConvertFrom(42), TEXT("42"));
	TestTrue(TEXT("float to string contains value"), FTypeOps<FString>::ConvertFrom(3.14f).Contains(TEXT("3.14")));

	// ConvertFrom FVector
	const FString FromVec = FTypeOps<FString>::ConvertFrom(FVector(1.0, 2.0, 3.0));
	TestTrue(TEXT("FVector to string contains X"), FromVec.Contains(TEXT("X=")));
	TestTrue(TEXT("FVector to string contains Y"), FromVec.Contains(TEXT("Y=")));

	// ConvertFrom FName
	const FString FromName = FTypeOps<FString>::ConvertFrom(FName(TEXT("TestName")));
	TestEqual(TEXT("FName to string"), FromName, TEXT("TestName"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsStringBlendTest,
	"PCGEx.Unit.Types.TypeOpsString.FString.Blend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsStringBlendTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FString A = TEXT("Hello");
	const FString B = TEXT("World");

	// Add (concatenation)
	TestEqual(TEXT("Add concatenates"), FTypeOps<FString>::Add(A, B), TEXT("HelloWorld"));

	// Sub (replace with empty)
	const FString WithSub = TEXT("Hello World");
	TestEqual(TEXT("Sub removes substring"), FTypeOps<FString>::Sub(WithSub, TEXT("World")), TEXT("Hello "));

	// Mult (same as Add)
	TestEqual(TEXT("Mult concatenates"), FTypeOps<FString>::Mult(A, B), TEXT("HelloWorld"));

	// Div returns original
	TestEqual(TEXT("Div returns original"), FTypeOps<FString>::Div(A, 2.0), A);

	// Lerp (threshold-based)
	TestEqual(TEXT("Lerp < 0.5 returns A"), FTypeOps<FString>::Lerp(A, B, 0.3), A);
	TestEqual(TEXT("Lerp >= 0.5 returns B"), FTypeOps<FString>::Lerp(A, B, 0.7), B);

	// Min/Max by length
	const FString Short = TEXT("Hi");
	const FString Long = TEXT("Hello World");
	TestEqual(TEXT("Min returns shorter"), FTypeOps<FString>::Min(Short, Long), Short);
	TestEqual(TEXT("Max returns longer"), FTypeOps<FString>::Max(Short, Long), Long);

	// Average (concatenation with separator)
	TestEqual(TEXT("Average joins with |"), FTypeOps<FString>::Average(A, B), TEXT("Hello|World"));

	// WeightedAdd
	TestEqual(TEXT("WeightedAdd W<=0.5 returns A"), FTypeOps<FString>::WeightedAdd(A, B, 0.3), A);
	TestEqual(TEXT("WeightedAdd W>0.5 concatenates"), FTypeOps<FString>::WeightedAdd(A, B, 0.7), TEXT("HelloWorld"));

	// Copy
	TestEqual(TEXT("CopyA returns A"), FTypeOps<FString>::CopyA(A, B), A);
	TestEqual(TEXT("CopyB returns B"), FTypeOps<FString>::CopyB(A, B), B);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsStringHashTest,
	"PCGEx.Unit.Types.TypeOpsString.FString.Hash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsStringHashTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FString A = TEXT("Alpha");
	const FString B = TEXT("Beta");

	const uint32 Hash = FTypeOps<FString>::Hash(A);
	TestTrue(TEXT("Hash returns non-zero"), Hash != 0);

	const FString NaiveH = FTypeOps<FString>::NaiveHash(A, B);
	TestTrue(TEXT("NaiveHash returns numeric string"), !NaiveH.IsEmpty());

	const FString UnsignedH = FTypeOps<FString>::UnsignedHash(A, B);
	const FString UnsignedHRev = FTypeOps<FString>::UnsignedHash(B, A);
	TestEqual(TEXT("UnsignedHash is order-independent"), UnsignedH, UnsignedHRev);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsStringMiscTest,
	"PCGEx.Unit.Types.TypeOpsString.FString.Misc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsStringMiscTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FString A = TEXT("Test");

	// Mod operations return original for strings
	TestEqual(TEXT("ModSimple returns original"), FTypeOps<FString>::ModSimple(A, 10.0), A);
	TestEqual(TEXT("ModComplex returns original"), FTypeOps<FString>::ModComplex(A, TEXT("X")), A);

	// Abs and Factor return original for strings
	TestEqual(TEXT("Abs returns original"), FTypeOps<FString>::Abs(A), A);
	TestEqual(TEXT("Factor returns original"), FTypeOps<FString>::Factor(A, 2.0), A);

	// NormalizeWeight returns original
	TestEqual(TEXT("NormalizeWeight returns original"), FTypeOps<FString>::NormalizeWeight(A, 5.0), A);

	// ExtractField returns 0 for strings
	TestEqual(TEXT("ExtractField returns 0"), FTypeOps<FString>::ExtractField(&A, ESingleField::X), 0.0);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FName Type Operations
//////////////////////////////////////////////////////////////////////////

#pragma region FName

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsNameDefaultTest,
	"PCGEx.Unit.Types.TypeOpsString.FName.GetDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsNameDefaultTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FName Default = FTypeOps<FName>::GetDefault();
	TestTrue(TEXT("Default name should be NAME_None"), Default == NAME_None);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsNameConversionsTest,
	"PCGEx.Unit.Types.TypeOpsString.FName.Conversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsNameConversionsTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FName SampleName = FName(TEXT("TestName"));

	// ConvertTo bool
	TestTrue(TEXT("Non-None name converts to true"), FTypeOps<FName>::ConvertTo<bool>(SampleName));
	TestFalse(TEXT("NAME_None converts to false"), FTypeOps<FName>::ConvertTo<bool>(NAME_None));

	// ConvertTo numeric (from string representation)
	const FName NumName = FName(TEXT("42"));
	TestEqual(TEXT("'42' name to int32"), FTypeOps<FName>::ConvertTo<int32>(NumName), 42);
	TestTrue(TEXT("'42' name to float"), FMath::IsNearlyEqual(FTypeOps<FName>::ConvertTo<float>(NumName), 42.0f));

	// ConvertTo FString
	const FString Str = FTypeOps<FName>::ConvertTo<FString>(SampleName);
	TestEqual(TEXT("FName to FString"), Str, TEXT("TestName"));

	// ConvertFrom bool
	TestEqual(TEXT("true to FName"), FTypeOps<FName>::ConvertFrom(true), FName(TEXT("true")));
	TestEqual(TEXT("false to FName"), FTypeOps<FName>::ConvertFrom(false), FName(TEXT("false")));

	// ConvertFrom numeric
	const FName From42 = FTypeOps<FName>::ConvertFrom(42);
	TestEqual(TEXT("int32 42 to FName"), From42.ToString(), TEXT("42"));

	// ConvertFrom FString
	TestEqual(TEXT("FString to FName"), FTypeOps<FName>::ConvertFrom(FString(TEXT("Hello"))), FName(TEXT("Hello")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsNameBlendTest,
	"PCGEx.Unit.Types.TypeOpsString.FName.Blend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsNameBlendTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FName A = FName(TEXT("Alpha"));
	const FName B = FName(TEXT("Beta"));

	// Add (concatenation)
	TestEqual(TEXT("Add concatenates"), FTypeOps<FName>::Add(A, B), FName(TEXT("AlphaBeta")));

	// Sub (removes substring)
	const FName WithSub = FName(TEXT("AlphaBeta"));
	TestEqual(TEXT("Sub removes substring"), FTypeOps<FName>::Sub(WithSub, B), FName(TEXT("Alpha")));

	// Lerp (threshold-based)
	TestEqual(TEXT("Lerp < 0.5 returns A"), FTypeOps<FName>::Lerp(A, B, 0.3), A);
	TestEqual(TEXT("Lerp >= 0.5 returns B"), FTypeOps<FName>::Lerp(A, B, 0.7), B);

	// Min/Max by string length
	const FName Short = FName(TEXT("Hi"));
	const FName Long = FName(TEXT("HelloWorld"));
	TestEqual(TEXT("Min returns shorter"), FTypeOps<FName>::Min(Short, Long), Short);
	TestEqual(TEXT("Max returns longer"), FTypeOps<FName>::Max(Short, Long), Long);

	// Average (concatenation with separator)
	TestEqual(TEXT("Average joins with _"), FTypeOps<FName>::Average(A, B), FName(TEXT("Alpha_Beta")));

	// Copy
	TestEqual(TEXT("CopyA returns A"), FTypeOps<FName>::CopyA(A, B), A);
	TestEqual(TEXT("CopyB returns B"), FTypeOps<FName>::CopyB(A, B), B);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsNameHashTest,
	"PCGEx.Unit.Types.TypeOpsString.FName.Hash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsNameHashTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FName A = FName(TEXT("Alpha"));
	const FName B = FName(TEXT("Beta"));

	const uint32 Hash = FTypeOps<FName>::Hash(A);
	TestTrue(TEXT("Hash returns non-zero"), Hash != 0);

	const FName NaiveH = FTypeOps<FName>::NaiveHash(A, B);
	TestTrue(TEXT("NaiveHash returns valid name"), NaiveH != NAME_None);

	const FName UnsignedH = FTypeOps<FName>::UnsignedHash(A, B);
	const FName UnsignedHRev = FTypeOps<FName>::UnsignedHash(B, A);
	TestEqual(TEXT("UnsignedHash is order-independent"), UnsignedH, UnsignedHRev);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FSoftObjectPath Type Operations
//////////////////////////////////////////////////////////////////////////

#pragma region FSoftObjectPath

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsSoftObjectPathDefaultTest,
	"PCGEx.Unit.Types.TypeOpsString.FSoftObjectPath.GetDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsSoftObjectPathDefaultTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FSoftObjectPath Default = FTypeOps<FSoftObjectPath>::GetDefault();
	TestFalse(TEXT("Default path should be invalid"), Default.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsSoftObjectPathConversionsTest,
	"PCGEx.Unit.Types.TypeOpsString.FSoftObjectPath.Conversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsSoftObjectPathConversionsTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FSoftObjectPath ValidPath(TEXT("/Game/Test/Asset.Asset"));
	const FSoftObjectPath InvalidPath;

	// ConvertTo bool
	TestTrue(TEXT("Valid path converts to true"), FTypeOps<FSoftObjectPath>::ConvertTo<bool>(ValidPath));
	TestFalse(TEXT("Invalid path converts to false"), FTypeOps<FSoftObjectPath>::ConvertTo<bool>(InvalidPath));

	// ConvertTo numeric (returns 0)
	TestEqual(TEXT("Path to int32 returns 0"), FTypeOps<FSoftObjectPath>::ConvertTo<int32>(ValidPath), 0);
	TestTrue(TEXT("Path to float returns 0"), FMath::IsNearlyZero(FTypeOps<FSoftObjectPath>::ConvertTo<float>(ValidPath)));

	// ConvertTo FString
	const FString Str = FTypeOps<FSoftObjectPath>::ConvertTo<FString>(ValidPath);
	TestEqual(TEXT("Path to FString"), Str, TEXT("/Game/Test/Asset.Asset"));

	// ConvertTo FName
	const FName Name = FTypeOps<FSoftObjectPath>::ConvertTo<FName>(ValidPath);
	TestTrue(TEXT("Path to FName not None"), Name != NAME_None);

	// ConvertTo FVector (returns zero)
	const FVector Vec = FTypeOps<FSoftObjectPath>::ConvertTo<FVector>(ValidPath);
	TestTrue(TEXT("Path to FVector is zero"), Vec.IsZero());

	// ConvertFrom FString
	const FSoftObjectPath FromStr = FTypeOps<FSoftObjectPath>::ConvertFrom(FString(TEXT("/Game/Another/Path")));
	TestEqual(TEXT("FString to Path"), FromStr.ToString(), TEXT("/Game/Another/Path"));

	// ConvertFrom numeric (returns invalid)
	const FSoftObjectPath FromInt = FTypeOps<FSoftObjectPath>::ConvertFrom(42);
	TestFalse(TEXT("int32 to Path is invalid"), FromInt.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsSoftObjectPathBlendTest,
	"PCGEx.Unit.Types.TypeOpsString.FSoftObjectPath.Blend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsSoftObjectPathBlendTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FSoftObjectPath A(TEXT("/Game/PathA"));
	const FSoftObjectPath B(TEXT("/Game/PathB"));
	const FSoftObjectPath Invalid;

	// Add (returns first valid)
	TestEqual(TEXT("Add returns A if valid"), FTypeOps<FSoftObjectPath>::Add(A, B).ToString(), A.ToString());
	TestEqual(TEXT("Add returns B if A invalid"), FTypeOps<FSoftObjectPath>::Add(Invalid, B).ToString(), B.ToString());

	// Sub returns A
	TestEqual(TEXT("Sub returns A"), FTypeOps<FSoftObjectPath>::Sub(A, B).ToString(), A.ToString());

	// Mult (returns A if both valid, else invalid)
	TestEqual(TEXT("Mult returns A if both valid"), FTypeOps<FSoftObjectPath>::Mult(A, B).ToString(), A.ToString());
	TestFalse(TEXT("Mult returns invalid if A invalid"), FTypeOps<FSoftObjectPath>::Mult(Invalid, B).IsValid());

	// Lerp (threshold-based)
	TestEqual(TEXT("Lerp < 0.5 returns A"), FTypeOps<FSoftObjectPath>::Lerp(A, B, 0.3).ToString(), A.ToString());
	TestEqual(TEXT("Lerp >= 0.5 returns B"), FTypeOps<FSoftObjectPath>::Lerp(A, B, 0.7).ToString(), B.ToString());

	// Min/Max by string comparison
	const FSoftObjectPath PathA(TEXT("/Game/A"));
	const FSoftObjectPath PathZ(TEXT("/Game/Z"));
	TestEqual(TEXT("Min returns alphabetically first"), FTypeOps<FSoftObjectPath>::Min(PathA, PathZ).ToString(), PathA.ToString());
	TestEqual(TEXT("Max returns alphabetically last"), FTypeOps<FSoftObjectPath>::Max(PathA, PathZ).ToString(), PathZ.ToString());

	// Copy
	TestEqual(TEXT("CopyA returns A"), FTypeOps<FSoftObjectPath>::CopyA(A, B).ToString(), A.ToString());
	TestEqual(TEXT("CopyB returns B"), FTypeOps<FSoftObjectPath>::CopyB(A, B).ToString(), B.ToString());

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FSoftClassPath Type Operations
//////////////////////////////////////////////////////////////////////////

#pragma region FSoftClassPath

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsSoftClassPathDefaultTest,
	"PCGEx.Unit.Types.TypeOpsString.FSoftClassPath.GetDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsSoftClassPathDefaultTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FSoftClassPath Default = FTypeOps<FSoftClassPath>::GetDefault();
	TestFalse(TEXT("Default class path should be invalid"), Default.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsSoftClassPathConversionsTest,
	"PCGEx.Unit.Types.TypeOpsString.FSoftClassPath.Conversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsSoftClassPathConversionsTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FSoftClassPath ValidPath(TEXT("/Script/Engine.Actor"));
	const FSoftClassPath InvalidPath;

	// ConvertTo bool
	TestTrue(TEXT("Valid class path converts to true"), FTypeOps<FSoftClassPath>::ConvertTo<bool>(ValidPath));
	TestFalse(TEXT("Invalid class path converts to false"), FTypeOps<FSoftClassPath>::ConvertTo<bool>(InvalidPath));

	// ConvertTo FString
	const FString Str = FTypeOps<FSoftClassPath>::ConvertTo<FString>(ValidPath);
	TestEqual(TEXT("ClassPath to FString"), Str, TEXT("/Script/Engine.Actor"));

	// ConvertTo FSoftObjectPath
	const FSoftObjectPath ObjPath = FTypeOps<FSoftClassPath>::ConvertTo<FSoftObjectPath>(ValidPath);
	TestEqual(TEXT("ClassPath to ObjectPath"), ObjPath.ToString(), TEXT("/Script/Engine.Actor"));

	// ConvertFrom FString
	const FSoftClassPath FromStr = FTypeOps<FSoftClassPath>::ConvertFrom(FString(TEXT("/Script/Core.Object")));
	TestEqual(TEXT("FString to ClassPath"), FromStr.ToString(), TEXT("/Script/Core.Object"));

	// ConvertFrom FSoftObjectPath
	const FSoftClassPath FromObjPath = FTypeOps<FSoftClassPath>::ConvertFrom(FSoftObjectPath(TEXT("/Script/Test.Class")));
	TestEqual(TEXT("ObjectPath to ClassPath"), FromObjPath.ToString(), TEXT("/Script/Test.Class"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsSoftClassPathBlendTest,
	"PCGEx.Unit.Types.TypeOpsString.FSoftClassPath.Blend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExTypeOpsSoftClassPathBlendTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTypeOps;

	const FSoftClassPath A(TEXT("/Script/A.ClassA"));
	const FSoftClassPath B(TEXT("/Script/B.ClassB"));
	const FSoftClassPath Invalid;

	// Add (returns first valid)
	TestEqual(TEXT("Add returns A if valid"), FTypeOps<FSoftClassPath>::Add(A, B).ToString(), A.ToString());
	TestEqual(TEXT("Add returns B if A invalid"), FTypeOps<FSoftClassPath>::Add(Invalid, B).ToString(), B.ToString());

	// Lerp (threshold-based)
	TestEqual(TEXT("Lerp < 0.5 returns A"), FTypeOps<FSoftClassPath>::Lerp(A, B, 0.3).ToString(), A.ToString());
	TestEqual(TEXT("Lerp >= 0.5 returns B"), FTypeOps<FSoftClassPath>::Lerp(A, B, 0.7).ToString(), B.ToString());

	// Copy
	TestEqual(TEXT("CopyA returns A"), FTypeOps<FSoftClassPath>::CopyA(A, B).ToString(), A.ToString());
	TestEqual(TEXT("CopyB returns B"), FTypeOps<FSoftClassPath>::CopyB(A, B).ToString(), B.ToString());

	return true;
}

#pragma endregion
