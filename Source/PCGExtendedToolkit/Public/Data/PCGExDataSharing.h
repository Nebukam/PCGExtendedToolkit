// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGData.h"
#include "PCGExSharedDataComponent.h"
#include "PCGPin.h"
#include "UObject/Object.h"
#include <functional>

#include "PCGExDataSharing.generated.h"

UENUM()
enum class EPCGExDataSharingPushType : uint8
{
	Replace = 0 UMETA(DisplayName = "Replace", Tooltip="Replace specified data in the target bucket."),
	Append  = 1 UMETA(DisplayName = "Append", Tooltip="Append specified data in the target bucket."),
	Remove  = 2 UMETA(DisplayName = "Remove", Tooltip="Removes specified data from the target bucket."),
};

UENUM(BlueprintType)
enum class EPCGExPinStatus : uint8 // needed for 5.3
{
	/** Normal usage pin. */
	Normal = 0,
	/** If no data is present, prevent the node from executing. */
	Required
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSharedDataPin
{
	GENERATED_BODY()

	FPCGExSharedDataPin()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName Label = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExPinStatus Status = EPCGExPinStatus::Normal;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGDataType AllowedTypes = EPCGDataType::Any;
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSharedDataLookup
{
	GENERATED_BODY()

	FPCGExSharedDataLookup()
	{
	}

	/** Bucket ID */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName BucketId = FName("Global");

	/** Data ID to look for in the specified bucket */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName ItemId = FName("ItemId");
};

UCLASS(Hidden)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExDataOwnedItem : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TSoftObjectPtr<UPCGExSharedDataComponent> Owner;

	UPROPERTY()
	FPCGDataCollection Collection;
};

UCLASS(Hidden)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExDataBucket : public UObject
{
	GENERATED_BODY()

	mutable FRWLock ContentLock;

	bool bFlushing = false;

public:
	using FDataFilterFunc = std::function<bool(const FPCGTaggedData&)>;

	uint32 BucketId;

	UPROPERTY(Transient)
	TMap<uint32, FPCGDataCollection> Content;

	void Append(uint32 Item, const FPCGDataCollection& Data);
	void Remove(uint32 Item, const FPCGDataCollection& Data);
	void Replace(uint32 Item, const FPCGDataCollection& Data);

	int32 Grab(const uint32 Item, FPCGDataCollection& OutData, FDataFilterFunc&& Filter);

	void Flush();
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSharedDataRegistered, uint32, Id);

UCLASS(Hidden)
class UPCGExSharedDataManager : public UObject
{
	GENERATED_BODY()

	mutable FRWLock BucketLock;

public:
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnSharedDataRegistered OnSharedDataRegistered;
	
	UPROPERTY()
	TMap<uint32, TObjectPtr<UPCGExDataBucket>> Buckets;

	void PushData(uint32 BucketId, uint32 ItemId, const FPCGDataCollection& InCollection, EPCGExDataSharingPushType InPushType);

	UPCGExDataBucket* FindBucket(uint32 BucketId);

	void FlushBucket(uint32 BucketId);
	void Flush();
};
