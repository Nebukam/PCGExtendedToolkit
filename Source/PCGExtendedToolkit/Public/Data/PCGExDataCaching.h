// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGPointData.h"

namespace PCGExDataCaching
{
	class PCGEXTENDEDTOOLKIT_API FCacheBase
	{
		FName AttributeName = NAME_None;
		EPCGMetadataTypes Type = EPCGMetadataTypes::Unknown;

		uint64 UID()
		{
			return PCGEx::H64(GetTypeHash(AttributeName), static_cast<int32>(Type));
		}
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FCache
	{
	public:
		TArray<T> Values;

		~FCache()
		{
			Values.Empty();
		}
	};

	class PCGEXTENDEDTOOLKIT_API FPool
	{
	public:
		UPCGPointData* Source = nullptr;
		TArray<FCacheBase*> Caches;
		TMap<uint64, FCacheBase> CacheMap;
	};
}

namespace PCGExDataCachingTask
{
	class PCGEXTENDEDTOOLKIT_API FBlendCompoundedIO final : public PCGExMT::FPCGExTask
	{
	public:
		FBlendCompoundedIO(PCGExData::FPointIO* InPointIO,
		                   PCGExData::FPointIO* InTargetIO,
		                   FPCGExBlendingSettings* InBlendingSettings,
		                   PCGExData::FIdxCompoundList* InCompoundList,
		                   const FPCGExDistanceSettings& InDistSettings,
		                   PCGExGraph::FGraphMetadataSettings* InMetadataSettings = nullptr) :
			FPCGExTask(InPointIO),
			TargetIO(InTargetIO),
			BlendingSettings(InBlendingSettings),
			CompoundList(InCompoundList),
			DistSettings(InDistSettings),
			MetadataSettings(InMetadataSettings)
		{
		}

		PCGExData::FPointIO* TargetIO = nullptr;
		FPCGExBlendingSettings* BlendingSettings = nullptr;
		PCGExData::FIdxCompoundList* CompoundList = nullptr;
		FPCGExDistanceSettings DistSettings;
		PCGExGraph::FGraphMetadataSettings* MetadataSettings = nullptr;

		virtual bool ExecuteTask() override;
	};
}
