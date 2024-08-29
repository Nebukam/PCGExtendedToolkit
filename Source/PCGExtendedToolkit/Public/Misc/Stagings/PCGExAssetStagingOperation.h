// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExRandom.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Paths/SubPoints/PCGExSubPointsOperation.h"
#include "PCGExAssetStagingOperation.generated.h"

class UPCGExAssetCollection;
enum class EPCGExSeedComponents : uint8;
struct FPCGExAssetStagingData;
/**
 * 
 */
UCLASS(MinimalAPI, Abstract)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExAssetStagingOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	UPCGExAssetCollection* SourceCollection = nullptr;
	uint8 SeedComponents = 0;
	int32 LocalSeed = 0;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual void GetStaging(
		FPCGExAssetStagingData& OutStaging,
		const PCGExData::FPointRef& Point,
		const int32 Index,
		const UPCGSettings* Settings = nullptr,
		const UPCGComponent* Component = nullptr) const;

};
