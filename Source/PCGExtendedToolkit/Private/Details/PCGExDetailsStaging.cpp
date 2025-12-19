// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Details/PCGExDetailsStaging.h"

#include "UObject/Object.h"
#include "UObject/Package.h"
#include "Collections/Core/PCGExAssetCollection.h"
#include "Collections/Core/PCGExCollectionHelpers.h"
#include "Details/PCGExDetailsDistances.h"
#include "Details/PCGExDetailsSettings.h"

FPCGExEntryTypeDetails::FPCGExEntryTypeDetails()
{
	EntryTypes = TSoftObjectPtr<UPCGExBitmaskCollection>(
		FSoftObjectPath(TEXT("/PCGExtendedToolkit/Data/Bitmasks/PCGEx_CollectionEntryTypes.PCGEx_CollectionEntryTypes"))
	);
}

FPCGExAssetDistributionIndexDetails::FPCGExAssetDistributionIndexDetails()
{
	if (IndexSource.GetName() == FName("@Last")) { IndexSource.Update(TEXT("$Index")); }
}

PCGEX_SETTING_VALUE_IMPL_BOOL(FPCGExAssetDistributionIndexDetails, Index, int32, true, IndexSource, -1);
PCGEX_SETTING_VALUE_IMPL(FPCGExAssetDistributionDetails, Category, FName, CategoryInput, CategoryAttribute, Category);


bool FPCGExSocketOutputDetails::Init(FPCGExContext* InContext) const
{
	PCGEX_VALIDATE_NAME_C(InContext, SocketNameAttributeName)
	PCGEX_VALIDATE_NAME_C(InContext, SocketTagAttributeName)
	PCGEX_VALIDATE_NAME_C(InContext, CategoryAttributeName)
	PCGEX_VALIDATE_NAME_C(InContext, AssetPathAttributeName)
	return true;
}

FPCGExRoamingAssetCollectionDetails::FPCGExRoamingAssetCollectionDetails(const TSubclassOf<UPCGExAssetCollection>& InAssetCollectionType)
	: bSupportCustomType(false), AssetCollectionType(InAssetCollectionType)
{
}

bool FPCGExRoamingAssetCollectionDetails::Validate(FPCGExContext* InContext) const
{
	if (!AssetCollectionType)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Collection type is not set."));
		return false;
	}

	return true;
}

UPCGExAssetCollection* FPCGExRoamingAssetCollectionDetails::TryBuildCollection(FPCGExContext* InContext, const UPCGParamData* InAttributeSet, const bool bBuildStaging) const
{
	if (!AssetCollectionType) { return nullptr; }
	UPCGExAssetCollection* Collection = InContext->ManagedObjects->New<UPCGExAssetCollection>(GetTransientPackage(), AssetCollectionType.Get(), NAME_None);
	if (!Collection) { return nullptr; }

	if (!PCGExCollectionHelpers::BuildFromAttributeSet(Collection, InContext, InAttributeSet, *this, bBuildStaging))
	{
		InContext->ManagedObjects->Destroy(Collection);
		return nullptr;
	}

	return Collection;
}

UPCGExAssetCollection* FPCGExRoamingAssetCollectionDetails::TryBuildCollection(FPCGExContext* InContext, const FName InputPin, const bool bBuildStaging) const
{
	if (!AssetCollectionType) { return nullptr; }
	UPCGExAssetCollection* Collection = InContext->ManagedObjects->New<UPCGExAssetCollection>(GetTransientPackage(), AssetCollectionType.Get(), NAME_None);
	if (!Collection) { return nullptr; }

	if (!PCGExCollectionHelpers::BuildFromAttributeSet(Collection, InContext, InputPin, *this, bBuildStaging))
	{
		InContext->ManagedObjects->Destroy(Collection);
		return nullptr;
	}

	return Collection;
}
