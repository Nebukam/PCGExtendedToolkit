// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/AutomationTest.h"
#include "Helpers/PCGExMetaHelpers.h"

/**
 * Tests for PCGExMetaHelpers.h
 * Covers: IsPCGExAttribute, MakePCGExAttributeName, IsWritableAttributeName,
 *         StringTagFromName, IsValidStringTag, IsDataDomainAttribute, StripDomainFromName,
 *         GetPropertyType, GetPropertyNativeTypes
 */

//////////////////////////////////////////////////////////////////////////
// IsPCGExAttribute Tests
//////////////////////////////////////////////////////////////////////////

#pragma region IsPCGExAttribute

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMetaHelpersIsPCGExAttributeStringTest,
	"PCGEx.Unit.Helpers.MetaHelpers.IsPCGExAttribute.String",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMetaHelpersIsPCGExAttributeStringTest::RunTest(const FString& Parameters)
{
	// PCGEx attributes contain "PCGEx/" prefix (defined in PCGExCommon::PCGExPrefix)
	TestTrue(TEXT("PCGEx/ prefixed string is PCGEx attribute"), PCGExMetaHelpers::IsPCGExAttribute(FString(TEXT("PCGEx/Test"))));
	TestTrue(TEXT("PCGEx/ alone is PCGEx attribute"), PCGExMetaHelpers::IsPCGExAttribute(FString(TEXT("PCGEx/"))));
	TestTrue(TEXT("String containing PCGEx/ is PCGEx attribute"), PCGExMetaHelpers::IsPCGExAttribute(FString(TEXT("SomePCGEx/Thing"))));

	TestFalse(TEXT("Regular attribute is not PCGEx"), PCGExMetaHelpers::IsPCGExAttribute(FString(TEXT("MyAttribute"))));
	TestFalse(TEXT("Empty string is not PCGEx"), PCGExMetaHelpers::IsPCGExAttribute(FString(TEXT(""))));
	TestFalse(TEXT("PCGEx without slash is not PCGEx"), PCGExMetaHelpers::IsPCGExAttribute(FString(TEXT("PCGEx"))));
	// Note: Contains is case-insensitive by default, so pcgex/ matches PCGEx/
	TestTrue(TEXT("pcgex/ lowercase is also PCGEx (case-insensitive)"), PCGExMetaHelpers::IsPCGExAttribute(FString(TEXT("pcgex/test"))));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMetaHelpersIsPCGExAttributeNameTest,
	"PCGEx.Unit.Helpers.MetaHelpers.IsPCGExAttribute.FName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMetaHelpersIsPCGExAttributeNameTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("PCGEx/ FName is PCGEx attribute"), PCGExMetaHelpers::IsPCGExAttribute(FName(TEXT("PCGEx/Test"))));
	TestFalse(TEXT("Regular FName is not PCGEx"), PCGExMetaHelpers::IsPCGExAttribute(FName(TEXT("MyAttribute"))));
	TestFalse(TEXT("NAME_None is not PCGEx"), PCGExMetaHelpers::IsPCGExAttribute(NAME_None));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMetaHelpersIsPCGExAttributeTextTest,
	"PCGEx.Unit.Helpers.MetaHelpers.IsPCGExAttribute.FText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMetaHelpersIsPCGExAttributeTextTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("PCGEx/ FText is PCGEx attribute"), PCGExMetaHelpers::IsPCGExAttribute(FText::FromString(TEXT("PCGEx/Test"))));
	TestFalse(TEXT("Regular FText is not PCGEx"), PCGExMetaHelpers::IsPCGExAttribute(FText::FromString(TEXT("MyAttribute"))));
	TestFalse(TEXT("Empty FText is not PCGEx"), PCGExMetaHelpers::IsPCGExAttribute(FText::GetEmpty()));

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// MakePCGExAttributeName Tests
//////////////////////////////////////////////////////////////////////////

#pragma region MakePCGExAttributeName

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMetaHelpersMakePCGExAttributeNameSingleTest,
	"PCGEx.Unit.Helpers.MetaHelpers.MakePCGExAttributeName.Single",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMetaHelpersMakePCGExAttributeNameSingleTest::RunTest(const FString& Parameters)
{
	const FName Result = PCGExMetaHelpers::MakePCGExAttributeName(TEXT("Test"));
	TestTrue(TEXT("Result starts with PCGEx/"), Result.ToString().StartsWith(TEXT("PCGEx/")));
	TestTrue(TEXT("Result contains input"), Result.ToString().Contains(TEXT("Test")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMetaHelpersMakePCGExAttributeNameDoubleTest,
	"PCGEx.Unit.Helpers.MetaHelpers.MakePCGExAttributeName.Double",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMetaHelpersMakePCGExAttributeNameDoubleTest::RunTest(const FString& Parameters)
{
	const FName Result = PCGExMetaHelpers::MakePCGExAttributeName(TEXT("Part1"), TEXT("Part2"));
	TestTrue(TEXT("Result starts with PCGEx/"), Result.ToString().StartsWith(TEXT("PCGEx/")));
	TestTrue(TEXT("Result contains Part1"), Result.ToString().Contains(TEXT("Part1")));
	TestTrue(TEXT("Result contains Part2"), Result.ToString().Contains(TEXT("Part2")));

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// IsWritableAttributeName Tests
//////////////////////////////////////////////////////////////////////////

#pragma region IsWritableAttributeName

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMetaHelpersIsWritableAttributeNameTest,
	"PCGEx.Unit.Helpers.MetaHelpers.IsWritableAttributeName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMetaHelpersIsWritableAttributeNameTest::RunTest(const FString& Parameters)
{
	// Valid writable names
	TestTrue(TEXT("Regular name is writable"), PCGExMetaHelpers::IsWritableAttributeName(FName(TEXT("MyAttribute"))));
	TestTrue(TEXT("Alphanumeric name is writable"), PCGExMetaHelpers::IsWritableAttributeName(FName(TEXT("Attr123"))));
	TestTrue(TEXT("Name with underscore is writable"), PCGExMetaHelpers::IsWritableAttributeName(FName(TEXT("My_Attribute"))));

	// Invalid names
	TestFalse(TEXT("NAME_None is not writable"), PCGExMetaHelpers::IsWritableAttributeName(NAME_None));
	TestFalse(TEXT("'None' string is not writable"), PCGExMetaHelpers::IsWritableAttributeName(FName(TEXT("None"))));

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// StringTagFromName Tests
//////////////////////////////////////////////////////////////////////////

#pragma region StringTagFromName

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMetaHelpersStringTagFromNameTest,
	"PCGEx.Unit.Helpers.MetaHelpers.StringTagFromName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMetaHelpersStringTagFromNameTest::RunTest(const FString& Parameters)
{
	const FString Result = PCGExMetaHelpers::StringTagFromName(FName(TEXT("TestName")));
	TestFalse(TEXT("Result is not empty"), Result.IsEmpty());
	TestTrue(TEXT("Result contains the name"), Result.Contains(TEXT("TestName")));

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// IsDataDomainAttribute Tests
//////////////////////////////////////////////////////////////////////////

#pragma region IsDataDomainAttribute

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMetaHelpersIsDataDomainAttributeNameTest,
	"PCGEx.Unit.Helpers.MetaHelpers.IsDataDomainAttribute.FName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMetaHelpersIsDataDomainAttributeNameTest::RunTest(const FString& Parameters)
{
	// Data domain attributes start with @Data.
	TestTrue(TEXT("@Data.Attr is data domain"), PCGExMetaHelpers::IsDataDomainAttribute(FName(TEXT("@Data.MyAttr"))));
	TestFalse(TEXT("@Elements.Attr is not data domain"), PCGExMetaHelpers::IsDataDomainAttribute(FName(TEXT("@Elements.MyAttr"))));
	TestFalse(TEXT("Regular attr is not data domain"), PCGExMetaHelpers::IsDataDomainAttribute(FName(TEXT("MyAttr"))));
	TestFalse(TEXT("NAME_None is not data domain"), PCGExMetaHelpers::IsDataDomainAttribute(NAME_None));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMetaHelpersIsDataDomainAttributeStringTest,
	"PCGEx.Unit.Helpers.MetaHelpers.IsDataDomainAttribute.String",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMetaHelpersIsDataDomainAttributeStringTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("@Data.Attr string is data domain"), PCGExMetaHelpers::IsDataDomainAttribute(FString(TEXT("@Data.MyAttr"))));
	TestFalse(TEXT("Regular string is not data domain"), PCGExMetaHelpers::IsDataDomainAttribute(FString(TEXT("MyAttr"))));
	TestFalse(TEXT("Empty string is not data domain"), PCGExMetaHelpers::IsDataDomainAttribute(FString(TEXT(""))));

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// StripDomainFromName Tests
//////////////////////////////////////////////////////////////////////////

#pragma region StripDomainFromName

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMetaHelpersStripDomainFromNameTest,
	"PCGEx.Unit.Helpers.MetaHelpers.StripDomainFromName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMetaHelpersStripDomainFromNameTest::RunTest(const FString& Parameters)
{
	// Strip @Data. prefix
	const FName DataStripped = PCGExMetaHelpers::StripDomainFromName(FName(TEXT("@Data.MyAttr")));
	TestEqual(TEXT("@Data. stripped"), DataStripped, FName(TEXT("MyAttr")));

	// Strip @Elements. prefix
	const FName ElementsStripped = PCGExMetaHelpers::StripDomainFromName(FName(TEXT("@Elements.MyAttr")));
	TestEqual(TEXT("@Elements. stripped"), ElementsStripped, FName(TEXT("MyAttr")));

	// No prefix - unchanged
	const FName NoPrefix = PCGExMetaHelpers::StripDomainFromName(FName(TEXT("MyAttr")));
	TestEqual(TEXT("No prefix unchanged"), NoPrefix, FName(TEXT("MyAttr")));

	// NAME_None - should handle gracefully
	const FName NoneResult = PCGExMetaHelpers::StripDomainFromName(NAME_None);
	TestEqual(TEXT("NAME_None unchanged"), NoneResult, NAME_None);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// GetPropertyType Tests (constexpr)
//////////////////////////////////////////////////////////////////////////

#pragma region GetPropertyType

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMetaHelpersGetPropertyTypeTest,
	"PCGEx.Unit.Helpers.MetaHelpers.GetPropertyType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMetaHelpersGetPropertyTypeTest::RunTest(const FString& Parameters)
{
	// Float properties
	TestEqual(TEXT("Density is Float"), PCGExMetaHelpers::GetPropertyType(EPCGPointProperties::Density), EPCGMetadataTypes::Float);
	TestEqual(TEXT("Steepness is Float"), PCGExMetaHelpers::GetPropertyType(EPCGPointProperties::Steepness), EPCGMetadataTypes::Float);

	// Vector properties
	TestEqual(TEXT("BoundsMin is Vector"), PCGExMetaHelpers::GetPropertyType(EPCGPointProperties::BoundsMin), EPCGMetadataTypes::Vector);
	TestEqual(TEXT("BoundsMax is Vector"), PCGExMetaHelpers::GetPropertyType(EPCGPointProperties::BoundsMax), EPCGMetadataTypes::Vector);
	TestEqual(TEXT("Extents is Vector"), PCGExMetaHelpers::GetPropertyType(EPCGPointProperties::Extents), EPCGMetadataTypes::Vector);
	TestEqual(TEXT("Position is Vector"), PCGExMetaHelpers::GetPropertyType(EPCGPointProperties::Position), EPCGMetadataTypes::Vector);
	TestEqual(TEXT("Scale is Vector"), PCGExMetaHelpers::GetPropertyType(EPCGPointProperties::Scale), EPCGMetadataTypes::Vector);
	TestEqual(TEXT("LocalCenter is Vector"), PCGExMetaHelpers::GetPropertyType(EPCGPointProperties::LocalCenter), EPCGMetadataTypes::Vector);
	TestEqual(TEXT("LocalSize is Vector"), PCGExMetaHelpers::GetPropertyType(EPCGPointProperties::LocalSize), EPCGMetadataTypes::Vector);
	TestEqual(TEXT("ScaledLocalSize is Vector"), PCGExMetaHelpers::GetPropertyType(EPCGPointProperties::ScaledLocalSize), EPCGMetadataTypes::Vector);

	// Vector4
	TestEqual(TEXT("Color is Vector4"), PCGExMetaHelpers::GetPropertyType(EPCGPointProperties::Color), EPCGMetadataTypes::Vector4);

	// Quaternion
	TestEqual(TEXT("Rotation is Quaternion"), PCGExMetaHelpers::GetPropertyType(EPCGPointProperties::Rotation), EPCGMetadataTypes::Quaternion);

	// Transform
	TestEqual(TEXT("Transform is Transform"), PCGExMetaHelpers::GetPropertyType(EPCGPointProperties::Transform), EPCGMetadataTypes::Transform);

	// Integer
	TestEqual(TEXT("Seed is Integer32"), PCGExMetaHelpers::GetPropertyType(EPCGPointProperties::Seed), EPCGMetadataTypes::Integer32);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMetaHelpersGetExtraPropertyTypeTest,
	"PCGEx.Unit.Helpers.MetaHelpers.GetPropertyType.Extra",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMetaHelpersGetExtraPropertyTypeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Index is Integer32"), PCGExMetaHelpers::GetPropertyType(EPCGExtraProperties::Index), EPCGMetadataTypes::Integer32);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// GetPropertyNativeTypes Tests (constexpr)
//////////////////////////////////////////////////////////////////////////

#pragma region GetPropertyNativeTypes

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMetaHelpersGetPropertyNativeTypesTest,
	"PCGEx.Unit.Helpers.MetaHelpers.GetPropertyNativeTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMetaHelpersGetPropertyNativeTypesTest::RunTest(const FString& Parameters)
{
	// Direct native properties
	TestEqual(TEXT("Density native type"), PCGExMetaHelpers::GetPropertyNativeTypes(EPCGPointProperties::Density), EPCGPointNativeProperties::Density);
	TestEqual(TEXT("BoundsMin native type"), PCGExMetaHelpers::GetPropertyNativeTypes(EPCGPointProperties::BoundsMin), EPCGPointNativeProperties::BoundsMin);
	TestEqual(TEXT("BoundsMax native type"), PCGExMetaHelpers::GetPropertyNativeTypes(EPCGPointProperties::BoundsMax), EPCGPointNativeProperties::BoundsMax);
	TestEqual(TEXT("Color native type"), PCGExMetaHelpers::GetPropertyNativeTypes(EPCGPointProperties::Color), EPCGPointNativeProperties::Color);
	TestEqual(TEXT("Steepness native type"), PCGExMetaHelpers::GetPropertyNativeTypes(EPCGPointProperties::Steepness), EPCGPointNativeProperties::Steepness);
	TestEqual(TEXT("Seed native type"), PCGExMetaHelpers::GetPropertyNativeTypes(EPCGPointProperties::Seed), EPCGPointNativeProperties::Seed);

	// Transform-based properties
	TestEqual(TEXT("Position uses Transform"), PCGExMetaHelpers::GetPropertyNativeTypes(EPCGPointProperties::Position), EPCGPointNativeProperties::Transform);
	TestEqual(TEXT("Rotation uses Transform"), PCGExMetaHelpers::GetPropertyNativeTypes(EPCGPointProperties::Rotation), EPCGPointNativeProperties::Transform);
	TestEqual(TEXT("Scale uses Transform"), PCGExMetaHelpers::GetPropertyNativeTypes(EPCGPointProperties::Scale), EPCGPointNativeProperties::Transform);
	TestEqual(TEXT("Transform uses Transform"), PCGExMetaHelpers::GetPropertyNativeTypes(EPCGPointProperties::Transform), EPCGPointNativeProperties::Transform);

	// Compound properties (BoundsMin | BoundsMax)
	const EPCGPointNativeProperties ExpectedBoundsBoth = EPCGPointNativeProperties::BoundsMin | EPCGPointNativeProperties::BoundsMax;
	TestEqual(TEXT("Extents uses both bounds"), PCGExMetaHelpers::GetPropertyNativeTypes(EPCGPointProperties::Extents), ExpectedBoundsBoth);
	TestEqual(TEXT("LocalCenter uses both bounds"), PCGExMetaHelpers::GetPropertyNativeTypes(EPCGPointProperties::LocalCenter), ExpectedBoundsBoth);
	TestEqual(TEXT("LocalSize uses both bounds"), PCGExMetaHelpers::GetPropertyNativeTypes(EPCGPointProperties::LocalSize), ExpectedBoundsBoth);

	// ScaledLocalSize uses bounds + transform
	const EPCGPointNativeProperties ExpectedScaledSize = EPCGPointNativeProperties::BoundsMin | EPCGPointNativeProperties::BoundsMax | EPCGPointNativeProperties::Transform;
	TestEqual(TEXT("ScaledLocalSize uses bounds + transform"), PCGExMetaHelpers::GetPropertyNativeTypes(EPCGPointProperties::ScaledLocalSize), ExpectedScaledSize);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// InvalidName Constant Test
//////////////////////////////////////////////////////////////////////////

#pragma region InvalidName

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMetaHelpersInvalidNameTest,
	"PCGEx.Unit.Helpers.MetaHelpers.InvalidName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMetaHelpersInvalidNameTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("InvalidName constant"), PCGExMetaHelpers::InvalidName, FName(TEXT("INVALID_DATA")));

	return true;
}

#pragma endregion
