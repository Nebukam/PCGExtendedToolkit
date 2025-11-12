// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Misc/ScopeRWLock.h"
#include "Templates/SharedPointer.h"
#include "Templates/SharedPointerFwd.h"
#include "UObject/SoftObjectPath.h"

#include "PCGExCommon.h"

#include "PCGEx.generated.h"

using PCGExTypeHash = uint32;
using InlineSparseAllocator = TSetAllocator<TSparseArrayAllocator<TInlineAllocator<8>, TInlineAllocator<8>>, TInlineAllocator<8>>;

UENUM()
enum class EPCGExOptionState : uint8
{
	Default  = 0 UMETA(DisplayName = "Default", Tooltip="Uses the default value selected in settings", ActionIcon="Default"),
	Enabled  = 1 UMETA(DisplayName = "Enabled", Tooltip="Option is enabled, if supported.", ActionIcon="Enabled"),
	Disabled = 2 UMETA(DisplayName = "Disabled", Tooltip="Option is disabled, if supported.", ActionIcon="Disabled")
};

UENUM()
enum class EPCGExAttributeSetPackingMode : uint8
{
	PerInput = 0 UMETA(DisplayName = "Per Input", ToolTip="..."),
	Merged   = 1 UMETA(DisplayName = "Merged", ToolTip="..."),
};

UENUM()
enum class EPCGExWinding : uint8
{
	Clockwise        = 1 UMETA(DisplayName = "Clockwise", ToolTip="Clockwise", ActionIcon="CW"),
	CounterClockwise = 2 UMETA(DisplayName = "Counter Clockwise", ToolTip="Counter Clockwise", ActionIcon="CCW"),
};

UENUM()
enum class EPCGExWindingMutation : uint8
{
	Unchanged        = 0 UMETA(DisplayName = "Unchanged", ToolTip="Unchanged", ActionIcon="Unchanged"),
	Clockwise        = 1 UMETA(DisplayName = "Clockwise", ToolTip="Clockwise", ActionIcon="CW"),
	CounterClockwise = 2 UMETA(DisplayName = "CounterClockwise", ToolTip="Counter Clockwise", ActionIcon="CCW"),
};

namespace PCGEx
{
#if WITH_EDITOR
	const FString META_PCGExDocURL = TEXT("PCGExNodeLibraryDoc");
	const FString META_PCGExDocNodeLibraryBaseURL = TEXT("https://pcgex.gitbook.io/pcgex/node-library/");
#endif

	const FName DEPRECATED_NAME = TEXT("#DEPRECATED");

	const FName PreviousAttributeName = TEXT("#Previous");
	const FName PreviousNameAttributeName = TEXT("#PreviousName");

	const FName SourceTargetsLabel = TEXT("Targets");
	const FName SourceSourcesLabel = TEXT("Sources");
	const FName SourceBoundsLabel = TEXT("Bounds");

	const FName SourceAdditionalReq = TEXT("AdditionalRequirementsFilters");
	const FName SourcePerInputOverrides = TEXT("PerInputOverrides");

	const FName SourcePointFilters = TEXT("PointFilters");
	const FName SourceUseValueIfFilters = TEXT("UsableValueFilters");

	const FSoftObjectPath DefaultDotOverDistanceCurve = FSoftObjectPath(TEXT("/PCGExtendedToolkit/Curves/FC_PCGExGraphBalance_DistanceOnly.FC_PCGExGraphBalance_DistanceOnly"));
	const FSoftObjectPath WeightDistributionLinearInv = FSoftObjectPath(TEXT("/PCGExtendedToolkit/Curves/FC_PCGExWeightDistribution_Linear_Inv.FC_PCGExWeightDistribution_Linear_Inv"));
	const FSoftObjectPath WeightDistributionLinear = FSoftObjectPath(TEXT("/PCGExtendedToolkit/Curves/FC_PCGExWeightDistribution_Linear.FC_PCGExWeightDistribution_Linear"));
	const FSoftObjectPath WeightDistributionExpoInv = FSoftObjectPath(TEXT("/PCGExtendedToolkit/Curves/FC_PCGExWeightDistribution_Expo_Inv.FC_PCGExWeightDistribution_Expo_Inv"));
	const FSoftObjectPath WeightDistributionExpo = FSoftObjectPath(TEXT("/PCGExtendedToolkit/Curves/FC_PCGExWeightDistribution_Expo.FC_PCGExWeightDistribution_Expo"));
	const FSoftObjectPath SteepnessWeightCurve = FSoftObjectPath(TEXT("/PCGExtendedToolkit/Curves/FC_PCGExSteepness_Default.FC_PCGExSteepness_Default"));


	PCGEXTENDEDTOOLKIT_API
	bool IsPCGExAttribute(const FString& InStr);
	PCGEXTENDEDTOOLKIT_API
	bool IsPCGExAttribute(const FName InName);
	PCGEXTENDEDTOOLKIT_API
	bool IsPCGExAttribute(const FText& InText);

	static FName MakePCGExAttributeName(const FString& Str0) { return FName(FText::Format(FText::FromString(TEXT("{0}{1}")), FText::FromString(PCGExCommon::PCGExPrefix), FText::FromString(Str0)).ToString()); }

	static FName MakePCGExAttributeName(const FString& Str0, const FString& Str1) { return FName(FText::Format(FText::FromString(TEXT("{0}{1}/{2}")), FText::FromString(PCGExCommon::PCGExPrefix), FText::FromString(Str0), FText::FromString(Str1)).ToString()); }

	PCGEXTENDEDTOOLKIT_API
	bool IsWritableAttributeName(const FName Name);
	PCGEXTENDEDTOOLKIT_API
	FString StringTagFromName(const FName Name);
	PCGEXTENDEDTOOLKIT_API
	bool IsValidStringTag(const FString& Tag);

	PCGEXTENDEDTOOLKIT_API
	void ArrayOfIndices(TArray<int32>& OutArray, const int32 InNum, const int32 Offset = 0);
	PCGEXTENDEDTOOLKIT_API
	int32 ArrayOfIndices(TArray<int32>& OutArray, const TArrayView<const int8>& Mask, const int32 Offset, const bool bInvert = false);
	PCGEXTENDEDTOOLKIT_API
	int32 ArrayOfIndices(TArray<int32>& OutArray, const TBitArray<>& Mask, const int32 Offset, const bool bInvert = false);

	PCGEXTENDEDTOOLKIT_API
	FName GetCompoundName(const FName A, const FName B);
	PCGEXTENDEDTOOLKIT_API
	FName GetCompoundName(const FName A, const FName B, const FName C);

	PCGEXTENDEDTOOLKIT_API
	void ScopeIndices(const TArray<int32>& InIndices, TArray<uint64>& OutScopes);

	struct PCGEXTENDEDTOOLKIT_API FOpStats
	{
		int32 Count = 0;
		double Weight = 0;
	};
}
