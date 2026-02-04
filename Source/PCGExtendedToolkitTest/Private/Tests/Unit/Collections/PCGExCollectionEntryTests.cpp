// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/AutomationTest.h"
#include "Core/PCGExAssetCollection.h"
#include "Details/PCGExStagingDetails.h"

/**
 * Tests for PCGExAssetCollection entry types
 * Covers: FPCGExAssetStagingData, FPCGExAssetCollectionEntry, detail structs
 */

//////////////////////////////////////////////////////////////////////////
// FPCGExAssetStagingData Tests
//////////////////////////////////////////////////////////////////////////

#pragma region FPCGExAssetStagingData

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAssetStagingDataDefaultTest,
	"PCGEx.Unit.Collections.Entry.StagingData.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAssetStagingDataDefaultTest::RunTest(const FString& Parameters)
{
	FPCGExAssetStagingData StagingData;

	TestEqual(TEXT("InternalIndex is -1 by default"), StagingData.InternalIndex, -1);
	TestTrue(TEXT("Path is empty by default"), StagingData.Path.IsNull());
	TestTrue(TEXT("Sockets array is empty by default"), StagingData.Sockets.IsEmpty());
	TestEqual(TEXT("Bounds is invalid by default"), StagingData.Bounds.IsValid, static_cast<uint8>(0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAssetStagingDataFindSocketEmptyTest,
	"PCGEx.Unit.Collections.Entry.StagingData.FindSocket.Empty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAssetStagingDataFindSocketEmptyTest::RunTest(const FString& Parameters)
{
	FPCGExAssetStagingData StagingData;
	const FPCGExSocket* OutSocket = nullptr;

	// No sockets - should return false
	const bool bFound = StagingData.FindSocket(FName(TEXT("TestSocket")), OutSocket);

	TestFalse(TEXT("FindSocket returns false on empty"), bFound);
	TestTrue(TEXT("OutSocket is nullptr"), OutSocket == nullptr);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAssetStagingDataFindSocketByNameTest,
	"PCGEx.Unit.Collections.Entry.StagingData.FindSocket.ByName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAssetStagingDataFindSocketByNameTest::RunTest(const FString& Parameters)
{
	FPCGExAssetStagingData StagingData;

	// Add some sockets
	FPCGExSocket Socket1;
	Socket1.SocketName = FName(TEXT("Socket_A"));
	Socket1.Tag = TEXT("TagA");
	StagingData.Sockets.Add(Socket1);

	FPCGExSocket Socket2;
	Socket2.SocketName = FName(TEXT("Socket_B"));
	Socket2.Tag = TEXT("TagB");
	StagingData.Sockets.Add(Socket2);

	FPCGExSocket Socket3;
	Socket3.SocketName = FName(TEXT("Socket_C"));
	Socket3.Tag = TEXT("TagC");
	StagingData.Sockets.Add(Socket3);

	// Find existing socket
	const FPCGExSocket* OutSocket = nullptr;
	const bool bFound = StagingData.FindSocket(FName(TEXT("Socket_B")), OutSocket);

	TestTrue(TEXT("FindSocket returns true for existing socket"), bFound);
	TestTrue(TEXT("OutSocket is not nullptr"), OutSocket != nullptr);
	if (OutSocket)
	{
		TestEqual(TEXT("Found socket has correct name"), OutSocket->SocketName, FName(TEXT("Socket_B")));
		TestEqual(TEXT("Found socket has correct tag"), OutSocket->Tag, TEXT("TagB"));
	}

	// Find non-existing socket
	const FPCGExSocket* OutSocket2 = nullptr;
	const bool bFound2 = StagingData.FindSocket(FName(TEXT("Socket_D")), OutSocket2);

	TestFalse(TEXT("FindSocket returns false for non-existing socket"), bFound2);
	TestTrue(TEXT("OutSocket is nullptr for non-existing"), OutSocket2 == nullptr);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAssetStagingDataFindSocketByNameAndTagTest,
	"PCGEx.Unit.Collections.Entry.StagingData.FindSocket.ByNameAndTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAssetStagingDataFindSocketByNameAndTagTest::RunTest(const FString& Parameters)
{
	FPCGExAssetStagingData StagingData;

	// Add multiple sockets with same name but different tags
	FPCGExSocket Socket1;
	Socket1.SocketName = FName(TEXT("MultiSocket"));
	Socket1.Tag = TEXT("Version1");
	StagingData.Sockets.Add(Socket1);

	FPCGExSocket Socket2;
	Socket2.SocketName = FName(TEXT("MultiSocket"));
	Socket2.Tag = TEXT("Version2");
	StagingData.Sockets.Add(Socket2);

	FPCGExSocket Socket3;
	Socket3.SocketName = FName(TEXT("OtherSocket"));
	Socket3.Tag = TEXT("Version1");
	StagingData.Sockets.Add(Socket3);

	// Find socket by name and tag
	const FPCGExSocket* OutSocket = nullptr;
	const bool bFound = StagingData.FindSocket(FName(TEXT("MultiSocket")), TEXT("Version2"), OutSocket);

	TestTrue(TEXT("FindSocket returns true for matching name and tag"), bFound);
	TestTrue(TEXT("OutSocket is not nullptr"), OutSocket != nullptr);
	if (OutSocket)
	{
		TestEqual(TEXT("Found socket has correct name"), OutSocket->SocketName, FName(TEXT("MultiSocket")));
		TestEqual(TEXT("Found socket has correct tag"), OutSocket->Tag, TEXT("Version2"));
	}

	// Wrong tag
	const FPCGExSocket* OutSocket2 = nullptr;
	const bool bFound2 = StagingData.FindSocket(FName(TEXT("MultiSocket")), TEXT("Version3"), OutSocket2);

	TestFalse(TEXT("FindSocket returns false for wrong tag"), bFound2);

	// Wrong name
	const FPCGExSocket* OutSocket3 = nullptr;
	const bool bFound3 = StagingData.FindSocket(FName(TEXT("WrongSocket")), TEXT("Version1"), OutSocket3);

	TestFalse(TEXT("FindSocket returns false for wrong name"), bFound3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAssetStagingDataFindSocketFirstMatchTest,
	"PCGEx.Unit.Collections.Entry.StagingData.FindSocket.FirstMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAssetStagingDataFindSocketFirstMatchTest::RunTest(const FString& Parameters)
{
	FPCGExAssetStagingData StagingData;

	// Add duplicate named sockets (different tags)
	FPCGExSocket Socket1;
	Socket1.SocketName = FName(TEXT("DuplicateName"));
	Socket1.Tag = TEXT("First");
	StagingData.Sockets.Add(Socket1);

	FPCGExSocket Socket2;
	Socket2.SocketName = FName(TEXT("DuplicateName"));
	Socket2.Tag = TEXT("Second");
	StagingData.Sockets.Add(Socket2);

	// FindSocket by name only should return the first match
	const FPCGExSocket* OutSocket = nullptr;
	const bool bFound = StagingData.FindSocket(FName(TEXT("DuplicateName")), OutSocket);

	TestTrue(TEXT("FindSocket returns true"), bFound);
	TestTrue(TEXT("OutSocket is not nullptr"), OutSocket != nullptr);
	if (OutSocket)
	{
		TestEqual(TEXT("First matching socket is returned"), OutSocket->Tag, TEXT("First"));
	}

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FPCGExAssetCollectionEntry Tests
//////////////////////////////////////////////////////////////////////////

#pragma region FPCGExAssetCollectionEntry

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAssetCollectionEntryDefaultTest,
	"PCGEx.Unit.Collections.Entry.CollectionEntry.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAssetCollectionEntryDefaultTest::RunTest(const FString& Parameters)
{
	FPCGExAssetCollectionEntry Entry;

	TestEqual(TEXT("Weight is 1 by default"), Entry.Weight, 1);
	TestEqual(TEXT("Category is NAME_None by default"), Entry.Category, NAME_None);
	TestFalse(TEXT("bIsSubCollection is false by default"), Entry.bIsSubCollection);
	TestTrue(TEXT("Tags is empty by default"), Entry.Tags.IsEmpty());
	TestTrue(TEXT("InternalSubCollection is nullptr by default"), Entry.InternalSubCollection == nullptr);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAssetCollectionEntryTypeIdTest,
	"PCGEx.Unit.Collections.Entry.CollectionEntry.TypeId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAssetCollectionEntryTypeIdTest::RunTest(const FString& Parameters)
{
	FPCGExAssetCollectionEntry Entry;

	// Regular entry (not subcollection) should return None
	Entry.bIsSubCollection = false;
	TestEqual(TEXT("Non-subcollection entry returns None type"), Entry.GetTypeId(), PCGExAssetCollection::TypeIds::None);

	// Subcollection entry should return Base
	Entry.bIsSubCollection = true;
	TestEqual(TEXT("Subcollection entry returns Base type"), Entry.GetTypeId(), PCGExAssetCollection::TypeIds::Base);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAssetCollectionEntryHasValidSubCollectionTest,
	"PCGEx.Unit.Collections.Entry.CollectionEntry.HasValidSubCollection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAssetCollectionEntryHasValidSubCollectionTest::RunTest(const FString& Parameters)
{
	FPCGExAssetCollectionEntry Entry;

	// Not a subcollection
	Entry.bIsSubCollection = false;
	Entry.InternalSubCollection = nullptr;
	TestFalse(TEXT("Non-subcollection returns false"), Entry.HasValidSubCollection());

	// Is subcollection but no pointer
	Entry.bIsSubCollection = true;
	Entry.InternalSubCollection = nullptr;
	TestFalse(TEXT("Subcollection with nullptr returns false"), Entry.HasValidSubCollection());

	// Note: Can't test with valid pointer without creating actual UObject

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAssetCollectionEntryHasPropertyOverrideTest,
	"PCGEx.Unit.Collections.Entry.CollectionEntry.HasPropertyOverride",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAssetCollectionEntryHasPropertyOverrideTest::RunTest(const FString& Parameters)
{
	FPCGExAssetCollectionEntry Entry;

	// Default entry has no overrides
	TestFalse(TEXT("Default entry has no override for TestProperty"),
		Entry.HasPropertyOverride(FName(TEXT("TestProperty"))));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAssetCollectionEntryClearSubCollectionTest,
	"PCGEx.Unit.Collections.Entry.CollectionEntry.ClearSubCollection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAssetCollectionEntryClearSubCollectionTest::RunTest(const FString& Parameters)
{
	FPCGExAssetCollectionEntry Entry;

	// Set a subcollection flag (without actual UObject)
	Entry.bIsSubCollection = true;

	// Clear should reset InternalSubCollection to nullptr
	Entry.ClearSubCollection();
	TestTrue(TEXT("ClearSubCollection sets InternalSubCollection to nullptr"),
		Entry.InternalSubCollection == nullptr);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FPCGExAssetTaggingDetails Tests
//////////////////////////////////////////////////////////////////////////

#pragma region FPCGExAssetTaggingDetails

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAssetTaggingDetailsDefaultTest,
	"PCGEx.Unit.Collections.Entry.TaggingDetails.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAssetTaggingDetailsDefaultTest::RunTest(const FString& Parameters)
{
	FPCGExAssetTaggingDetails Details;

	// Default GrabTags should be Asset only
	TestEqual(TEXT("Default GrabTags is Asset"), Details.GrabTags,
		static_cast<uint8>(EPCGExAssetTagInheritance::Asset));
	TestTrue(TEXT("Default details are enabled"), Details.IsEnabled());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAssetTaggingDetailsIsEnabledTest,
	"PCGEx.Unit.Collections.Entry.TaggingDetails.IsEnabled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAssetTaggingDetailsIsEnabledTest::RunTest(const FString& Parameters)
{
	FPCGExAssetTaggingDetails Details;

	// Test with various GrabTags values
	Details.GrabTags = 0;
	TestFalse(TEXT("GrabTags 0 is disabled"), Details.IsEnabled());

	Details.GrabTags = static_cast<uint8>(EPCGExAssetTagInheritance::Asset);
	TestTrue(TEXT("GrabTags Asset is enabled"), Details.IsEnabled());

	Details.GrabTags = static_cast<uint8>(EPCGExAssetTagInheritance::Collection);
	TestTrue(TEXT("GrabTags Collection is enabled"), Details.IsEnabled());

	Details.GrabTags = static_cast<uint8>(EPCGExAssetTagInheritance::Asset) |
	                   static_cast<uint8>(EPCGExAssetTagInheritance::Collection);
	TestTrue(TEXT("GrabTags Asset|Collection is enabled"), Details.IsEnabled());

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FPCGExAssetDistributionDetails Tests
//////////////////////////////////////////////////////////////////////////

#pragma region FPCGExAssetDistributionDetails

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAssetDistributionDetailsDefaultTest,
	"PCGEx.Unit.Collections.Entry.DistributionDetails.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAssetDistributionDetailsDefaultTest::RunTest(const FString& Parameters)
{
	FPCGExAssetDistributionDetails Details;

	TestFalse(TEXT("bUseCategories is false by default"), Details.bUseCategories);
	TestEqual(TEXT("Distribution is WeightedRandom by default"), Details.Distribution,
		EPCGExDistribution::WeightedRandom);
	TestEqual(TEXT("LocalSeed is 0 by default"), Details.LocalSeed, 0);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FPCGExMicroCacheDistributionDetails Tests
//////////////////////////////////////////////////////////////////////////

#pragma region FPCGExMicroCacheDistributionDetails

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMicroCacheDistributionDetailsDefaultTest,
	"PCGEx.Unit.Collections.Entry.MicroCacheDistributionDetails.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMicroCacheDistributionDetailsDefaultTest::RunTest(const FString& Parameters)
{
	FPCGExMicroCacheDistributionDetails Details;

	TestEqual(TEXT("Distribution is WeightedRandom by default"), Details.Distribution,
		EPCGExDistribution::WeightedRandom);
	TestEqual(TEXT("LocalSeed is 0 by default"), Details.LocalSeed, 0);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FPCGExAssetAttributeSetDetails Tests
//////////////////////////////////////////////////////////////////////////

#pragma region FPCGExAssetAttributeSetDetails

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAssetAttributeSetDetailsDefaultTest,
	"PCGEx.Unit.Collections.Entry.AttributeSetDetails.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAssetAttributeSetDetailsDefaultTest::RunTest(const FString& Parameters)
{
	FPCGExAssetAttributeSetDetails Details;

	TestEqual(TEXT("AssetPathSourceAttribute is 'AssetPath' by default"),
		Details.AssetPathSourceAttribute, FName(TEXT("AssetPath")));
	TestEqual(TEXT("WeightSourceAttribute is NAME_None by default"),
		Details.WeightSourceAttribute, NAME_None);
	TestEqual(TEXT("CategorySourceAttribute is NAME_None by default"),
		Details.CategorySourceAttribute, NAME_None);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FPCGExAssetDistributionIndexDetails Tests
//////////////////////////////////////////////////////////////////////////

#pragma region FPCGExAssetDistributionIndexDetails

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAssetDistributionIndexDetailsDefaultTest,
	"PCGEx.Unit.Collections.Entry.DistributionIndexDetails.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAssetDistributionIndexDetailsDefaultTest::RunTest(const FString& Parameters)
{
	FPCGExAssetDistributionIndexDetails Details;

	TestEqual(TEXT("PickMode is Ascending by default"), Details.PickMode,
		EPCGExIndexPickMode::Ascending);
	TestEqual(TEXT("IndexSafety is Tile by default"), Details.IndexSafety,
		EPCGExIndexSafety::Tile);
	TestFalse(TEXT("bRemapIndexToCollectionSize is false by default"),
		Details.bRemapIndexToCollectionSize);
	TestEqual(TEXT("TruncateRemap is None by default"), Details.TruncateRemap,
		EPCGExTruncateMode::None);

	return true;
}

#pragma endregion
