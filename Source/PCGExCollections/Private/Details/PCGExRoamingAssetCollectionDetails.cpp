// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Details/PCGExRoamingAssetCollectionDetails.h"

#include "Containers/PCGExManagedObjects.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "Core/PCGExAssetCollection.h"
#include "Core/PCGExCollectionHelpers.h"
#include "Details/PCGExSettingsDetails.h"

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
