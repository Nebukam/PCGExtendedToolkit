// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Misc/ScopeRWLock.h"
#include "Templates/SharedPointer.h"
#include "Templates/SharedPointerFwd.h"
#include "UObject/SoftObjectPath.h"

#include "PCGExCommon.h"

#include "PCGEx.generated.h"

UENUM()
enum class EPCGExAttributeSetPackingMode : uint8
{
	PerInput = 0 UMETA(DisplayName = "Per Input", ToolTip="..."),
	Merged   = 1 UMETA(DisplayName = "Merged", ToolTip="..."),
};

namespace PCGEx
{

	const FName DEPRECATED_NAME = TEXT("#DEPRECATED");

	const FName SourceTargetsLabel = TEXT("Targets");
	const FName SourceSourcesLabel = TEXT("Sources");
	const FName SourceBoundsLabel = TEXT("Bounds");

	const FName SourceAdditionalReq = TEXT("AdditionalRequirementsFilters");
	const FName SourcePerInputOverrides = TEXT("PerInputOverrides");

	const FName SourcePointFilters = TEXT("PointFilters");
	const FName SourceUseValueIfFilters = TEXT("UsableValueFilters");

	PCGEXTENDEDTOOLKIT_API FName GetCompoundName(const FName A, const FName B);
	PCGEXTENDEDTOOLKIT_API FName GetCompoundName(const FName A, const FName B, const FName C);

	PCGEXTENDEDTOOLKIT_API void ScopeIndices(const TArray<int32>& InIndices, TArray<uint64>& OutScopes);

}
