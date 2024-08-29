// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/Stagings/PCGExAssetStagingOperation.h"

#include "Data/PCGExPointIO.h"

void UPCGExAssetStagingOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExAssetStagingOperation* TypedOther = Cast<UPCGExAssetStagingOperation>(Other))
	{
		SourceCollection = TypedOther->SourceCollection;
		LocalSeed = TypedOther->LocalSeed;
	}
}

void UPCGExAssetStagingOperation::GetStaging(FPCGExAssetStagingData& OutStaging, const PCGExData::FPointRef& Point, const int32 Index, const UPCGSettings* Settings, const UPCGComponent* Component) const
{
}
