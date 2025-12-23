// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCollectionsCommon.h"
#include "Data/Utils/PCGExDataFilterDetails.h"
#include "Core/PCGExAssetCollection.h"
#include "PCGExRoamingAssetCollectionDetails.generated.h"

class UPCGExBitmaskCollection;
class UPCGParamData;

USTRUCT(BlueprintType)
struct PCGEXCOLLECTIONS_API FPCGExRoamingAssetCollectionDetails : public FPCGExAssetAttributeSetDetails
{
	GENERATED_BODY()

	FPCGExRoamingAssetCollectionDetails() = default;

	explicit FPCGExRoamingAssetCollectionDetails(const TSubclassOf<UPCGExAssetCollection>& InAssetCollectionType);

	UPROPERTY()
	bool bSupportCustomType = true;

	/** Defines what type of temp collection to build from input attribute set */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, NoClear, Category = Settings, meta=(PCG_Overridable, EditCondition="bSupportCustomType", EditConditionHides, HideEditConditionToggle))
	TSubclassOf<UPCGExAssetCollection> AssetCollectionType;

	bool Validate(FPCGExContext* InContext) const;

	UPCGExAssetCollection* TryBuildCollection(FPCGExContext* InContext, const UPCGParamData* InAttributeSet, const bool bBuildStaging = false) const;
	UPCGExAssetCollection* TryBuildCollection(FPCGExContext* InContext, const FName InputPin, const bool bBuildStaging) const;
};
