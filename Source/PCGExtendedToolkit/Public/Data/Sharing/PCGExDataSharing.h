// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGData.h"
#include "UObject/Object.h"
#include <functional>

#include "PCGEx.h"
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

namespace PCGExDataSharing
{
	using FDataFilterFunc = std::function<bool(const FPCGTaggedData&)>;

	PCGEXTENDEDTOOLKIT_API
	uint32 GetPartitionIdx(const FVector& InPosition, const double PartitionSize);

	PCGEXTENDEDTOOLKIT_API
	uint32 GetPartitionIdx(uint32 InBaseID, const FVector& InPosition, const double PartitionSize);

	class FDataBucket : public TSharedFromThis<FDataBucket>
	{
	public:
		FDataBucket() = default;

		bool Set(const uint32 Key, const FPCGDataCollection& InValue);
		bool Add(const uint32 Key, const FPCGDataCollection& InValue);
		bool Remove(const uint32 Key);

		FPCGDataCollection* Find(const uint32 Key);
		FPCGDataCollection* Find(const uint32 Key, FBox WithinBounds);
		int32 Append(const uint32 Key, FPCGDataCollection& OutCollection);

		void Empty();
		TSharedPtr<PCGEx::FIndexedItemOctree> GetOctree();

	protected:
		FRWLock OctreeLock;
		TMap<uint32, FPCGDataCollection> Data;
		TSharedPtr<PCGEx::FIndexedItemOctree> Octree;

		void RebuildOctree();

		int32 RemovalsSinceLastUpdate = 0;
	};
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSharedDataPin
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
struct PCGEXTENDEDTOOLKIT_API FPCGExSharedDataLookup
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
