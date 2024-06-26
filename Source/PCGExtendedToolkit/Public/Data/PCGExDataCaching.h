// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGPointData.h"
#include "PCGEx.h"
#include "PCGExAttributeHelpers.h"
#include "PCGExMacros.h"
#include "PCGExMT.h"

namespace PCGExDataCaching
{
	static uint64 CacheUID(const FName FullName, const EPCGMetadataTypes Type)
	{
		return PCGEx::H64(GetTypeHash(FullName), static_cast<int32>(Type));
	};

	class PCGEXTENDEDTOOLKIT_API FCacheBase
	{
	protected:
		mutable FRWLock CacheLock;
		mutable FRWLock WriteLock;
		bool bInitialized = false;

		int32 ReadyNum = 0;

	public:
		FName FullName = NAME_None;
		EPCGMetadataTypes Type = EPCGMetadataTypes::Unknown;
		const uint64 UID;
		PCGExData::FPointIO* Source = nullptr;


		FCacheBase(const FName InFullName, const EPCGMetadataTypes InType):
			FullName(InFullName), Type(InType), UID(CacheUID(FullName, Type))
		{
		}

		virtual ~FCacheBase() = default;

		void IncrementWriteReadyNum();
		void ReadyWrite(PCGExMT::FTaskManager* AsyncManager = nullptr);

		virtual void Write(PCGExMT::FTaskManager* AsyncManager = nullptr);
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FCache : public FCacheBase
	{
	public:
		TArray<T> Values;

		T Min = T{};
		T Max = T{};

		PCGEx::TFAttributeReader<T>* Reader = nullptr;
		PCGEx::TFAttributeWriter<T>* Writer = nullptr;

		FCache(const FName InFullName, const EPCGMetadataTypes InType):
			FCacheBase(InFullName, InType)
		{
		}

		virtual ~FCache() override
		{
			Values.Empty();
			PCGEX_DELETE(Reader)
			PCGEX_DELETE(Writer)
		}

		PCGEx::TFAttributeReader<T>* PrepareReader()
		{
			{
				FReadScopeLock ReadScopeLock(CacheLock);
				if (bInitialized) { return Reader; }
			}

			{
				FWriteScopeLock WriteScopeLock(CacheLock);
				bInitialized = true;

				Reader = new PCGEx::TFAttributeReader<T>(FullName);
				Reader->Bind(Source);

				return Reader;
			}
		}

		PCGEx::TFAttributeWriter<T>* PrepareWriter(T DefaultValue, bool bAllowInterpolation, bool bUninitialized = false)
		{
			{
				FReadScopeLock ReadScopeLock(CacheLock);
				if (bInitialized) { return Writer; }
			}
			{
				FWriteScopeLock WriteScopeLock(CacheLock);

				bInitialized = true;

				Writer = new PCGEx::TFAttributeWriter<T>(FullName, DefaultValue, bAllowInterpolation);
				if (bUninitialized) { Writer->BindAndSetNumUninitialized(Source); }
				else { Writer->BindAndGet(Source); }
				return Writer;
			}
		}

		PCGEx::TFAttributeWriter<T>* PrepareWriter(bool bUninitialized = false)
		{
			{
				FReadScopeLock ReadScopeLock(CacheLock);
				if (bInitialized) { return Writer; }
			}
			{
				FWriteScopeLock WriteScopeLock(CacheLock);

				bInitialized = true;

				Writer = new PCGEx::TFAttributeWriter<T>(FullName);
				if (bUninitialized) { Writer->BindAndSetNumUninitialized(Source); }
				else { Writer->BindAndGet(Source); }
				return Writer;
			}
		}

		void Grab(PCGEx::FAttributeGetter<T>* Getter, bool bCaptureMinMax = false)
		{
			{
				FReadScopeLock ReadScopeLock(CacheLock);
				if (bInitialized) { return; }
			}
			{
				FWriteScopeLock WriteScopeLock(CacheLock);
				bInitialized = true;

				Getter->GrabAndDump(Source, Values, bCaptureMinMax, Min, Max);
			}
		}

		virtual void Write(PCGExMT::FTaskManager* AsyncManager = nullptr) override
		{
			check(Writer)
			if (AsyncManager) { PCGEX_ASYNC_WRITE_DELETE(AsyncManager, Writer); }
			else { Writer->Write(); }
		}
	};

	class PCGEXTENDEDTOOLKIT_API FPool
	{
		mutable FRWLock PoolLock;

	public:
		PCGExData::FPointIO* Source = nullptr;
		TArray<FCacheBase*> Caches;
		TMap<uint64, FCacheBase*> CacheMap;

		FCacheBase* TryGetCache(const uint64 UID);

		explicit FPool(PCGExData::FPointIO* InSource):
			Source(InSource)
		{
		}

		template <typename T>
		FCache<T>* TryGetCache(const FName FullName)
		{
			FCacheBase* Found = TryGetCache(CacheUID(FullName, PCGEx::GetMetadataType(T{})));
			if (!Found) { return nullptr; }
			return static_cast<FCache<T>*>(Found);
		}

		template <typename T>
		FCache<T>* GetOrCreateCache(FName FullName)
		{
			FCache<T>* NewCache = TryGetCache<T>(FullName);
			if (NewCache) { return NewCache; }

			{
				FWriteScopeLock WriteScopeLock(PoolLock);
				NewCache = new FCache<T>(FullName, PCGEx::GetMetadataType(T{}));
				NewCache->Source = Source;
				Caches.Add(NewCache);
				CacheMap.Add(NewCache->UID, NewCache);
				return NewCache;
			}
		}

		template <typename T>
		FCache<T>* GetOrCreateGetter(const FPCGAttributePropertyInputSelector& Selector, bool bCaptureMinMax = false)
		{
			PCGEx::FAttributeGetter<T>* Getter;

			switch (PCGEx::GetMetadataType(T{}))
			{
			default: ;
			case EPCGMetadataTypes::Integer64:
			case EPCGMetadataTypes::Float:
			case EPCGMetadataTypes::Vector2:
			case EPCGMetadataTypes::Vector4:
			case EPCGMetadataTypes::Rotator:
			case EPCGMetadataTypes::Quaternion:
			case EPCGMetadataTypes::Transform:
			case EPCGMetadataTypes::String:
			case EPCGMetadataTypes::Name:
			case EPCGMetadataTypes::Count:
			case EPCGMetadataTypes::Unknown:
				return nullptr;
			// TODO : Proper implementation, this is cursed
			case EPCGMetadataTypes::Double:
				Getter = static_cast<PCGEx::FAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalSingleFieldGetter()));
				break;
			case EPCGMetadataTypes::Integer32:
				Getter = static_cast<PCGEx::FAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalIntegerGetter()));
				break;
			case EPCGMetadataTypes::Vector:
				Getter = static_cast<PCGEx::FAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalVectorGetter()));
				break;
			case EPCGMetadataTypes::Boolean:
				Getter = static_cast<PCGEx::FAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalBoolGetter()));
				break;
			}

			Getter->Capture(Selector);
			if (!Getter->SoftGrab(Source))
			{
				PCGEX_DELETE(Getter)
				return nullptr;
			}

			FCache<T>* Cache = GetOrCreateCache<T>(Getter->FullName);
			Cache->Grab(Getter, bCaptureMinMax);
			PCGEX_DELETE(Getter)

			return Cache;
		}

		template <typename T>
		FCache<T>* GetOrCreateWriter(const FName Name, T DefaultValue, bool bAllowInterpolation, bool bUninitialized)
		{
			FCache<T>* Cache = GetOrCreateCache(Name, PCGEx::GetMetadataType(T{}));
			Cache->PrepareWriter(DefaultValue, bAllowInterpolation, bUninitialized);
			return Cache;
		}

		template <typename T>
		FCache<T>* GetOrCreateWriter(const FName Name, bool bUninitialized)
		{
			FCache<T>* Cache = GetOrCreateCache<T>(Name);
			Cache->PrepareWriter(bUninitialized);
			return Cache;
		}

		template <typename T>
		FCache<T>* GetOrCreateReader(const FName Name)
		{
			FCache<T>* Cache = GetOrCreateCache<T>(Name);
			Cache->PrepareReader();
			return Cache;
		}

		~FPool()
		{
			PCGEX_DELETE_TARRAY(Caches)
			CacheMap.Empty();
			Source = nullptr;
		}

		void Write(PCGExMT::FTaskManager* AsyncManager = nullptr)
		{
			for (FCacheBase* Cache : Caches) { Cache->Write(AsyncManager); }
		}
	};
}

namespace PCGExDataCachingTask
{
	/*
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
	*/
}
