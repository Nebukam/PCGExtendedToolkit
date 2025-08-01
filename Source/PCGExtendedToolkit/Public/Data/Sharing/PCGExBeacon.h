// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#endif

#include "PCGExDataSharing.h"
#include "Engine/DataAsset.h"

#include "PCGExBeacon.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeaconContentAdded, uint32, Id);

UCLASS(Abstract, BlueprintType, DisplayName="[PCGEx] Beacon")
class PCGEXTENDEDTOOLKIT_API UPCGExBeacon : public UDataAsset
{
	mutable FRWLock ContentLock;
	bool bFlushing = false;

	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnBeaconContentAdded OnBeaconContentAdded;

	// TODO : Add blueprint methods

	void InternalSet(const uint32 Key, const FPCGDataCollection& InData);
	void InternalAdd(const uint32 Key, const FPCGDataCollection& InData);
	void InternalRemove(const uint32 Key, const FPCGDataCollection& InData);

	int32 Grab(const uint32 Key, FPCGDataCollection& OutData, PCGExDataSharing::FDataFilterFunc&& Filter);
	int32 Grab(const uint32 Key, FPCGDataCollection& OutData);

	void Empty();

protected:
	TSharedPtr<PCGExDataSharing::FDataBucket> ContentMap;
	TMap<uint32, TSharedPtr<PCGExDataSharing::FDataBucket>> PartitionedContentMap;

	TSharedPtr<PCGExDataSharing::FDataBucket> GetContentMap();

	void EnsureSubsystemRegistration();

	void OnUpdate(UPCGComponent* InSource, uint32 Item) const;
};
