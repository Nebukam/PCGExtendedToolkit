// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointIO.h"
#include "Blending/PCGExDataBlending.h"
#include "Blending/PCGExDataBlendingOperations.h"
#include "Data/PCGPointData.h"
#include "UObject/Object.h"

namespace PCGExData
{
	PCGEX_ASYNC_STATE(State_MergingData);

#pragma region Compound

	struct PCGEXTENDEDTOOLKIT_API FIdxCompound
	{
		TArray<uint64> CompoundedPoints;
		TArray<double> Weights;

		FIdxCompound() { CompoundedPoints.Empty(); }

		~FIdxCompound()
		{
			CompoundedPoints.Empty();
			Weights.Empty();
		}

		bool ContainsIOIndex(int32 InIOIndex);
		void ComputeWeights(const TArray<FPointIO*>& Sources, const FPCGPoint& Target, const FPCGExDistanceSettings& DistSettings);
		void ComputeWeights(const TArray<FPCGPoint>& SourcePoints, const FPCGPoint& Target, const FPCGExDistanceSettings& DistSettings);

		uint64 Add(const int32 IOIndex, const int32 PointIndex);

		int32 Num() const { return CompoundedPoints.Num(); }
		uint64 operator[](const int32 Index) const { return this->CompoundedPoints[Index]; }
	};

	struct PCGEXTENDEDTOOLKIT_API FIdxCompoundList
	{
		TArray<FIdxCompound*> Compounds;

		FIdxCompoundList() { Compounds.Empty(); }
		~FIdxCompoundList() { PCGEX_DELETE_TARRAY(Compounds) }

		int32 Num() const { return Compounds.Num(); }

		FIdxCompound* New();

		FORCEINLINE uint64 Add(const int32 Index, const int32 IOIndex, const int32 PointIndex);
		void GetIOIndices(const int32 Index, TArray<int32>& OutIOIndices);
		FORCEINLINE bool HasIOIndexOverlap(int32 InIdx, const TArray<int32>& InIndices);

		FIdxCompound* operator[](const int32 Index) const { return this->Compounds[Index]; }
	};

#pragma endregion

#pragma region Data Marking

	template <typename T>
	static FPCGMetadataAttribute<T>* WriteMark(UPCGMetadata* Metadata, const FName MarkID, T MarkValue)
	{
		Metadata->DeleteAttribute(MarkID);
		FPCGMetadataAttribute<T>* Mark = Metadata->CreateAttribute<T>(MarkID, MarkValue, false, true);
		Mark->SetDefaultValue(MarkValue);
		return Mark;
	}

	template <typename T>
	static FPCGMetadataAttribute<T>* WriteMark(const FPointIO* PointIO, const FName MarkID, T MarkValue)
	{
		return WriteMark(PointIO->GetOut()->Metadata, MarkID, MarkValue);
	}


	template <typename T>
	static bool TryReadMark(UPCGMetadata* Metadata, const FName MarkID, T& OutMark)
	{
		const FPCGMetadataAttribute<T>* Mark = Metadata->GetConstTypedAttribute<T>(MarkID);
		if (!Mark) { return false; }
		OutMark = Mark->GetValue(PCGInvalidEntryKey);
		return true;
	}

	template <typename T>
	static bool TryReadMark(const FPointIO& PointIO, const FName MarkID, T& OutMark)
	{
		return TryReadMark(PointIO.GetIn() ? PointIO.GetIn()->Metadata : PointIO.GetOut()->Metadata, MarkID, OutMark);
	}

	static void WriteId(const FPointIO& PointIO, const FName IdName, const int64 Id)
	{
		FString OutId;
		PointIO.Tags->Set(IdName.ToString(), Id, OutId);
		if (PointIO.GetOut()) { WriteMark(PointIO.GetOut()->Metadata, IdName, Id); }
	}

	static UPCGPointData* GetMutablePointData(FPCGContext* Context, const FPCGTaggedData& Source)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Source.Data);
		if (!SpatialData) { return nullptr; }

		const UPCGPointData* PointData = SpatialData->ToPointData(Context);
		if (!PointData) { return nullptr; }

		return const_cast<UPCGPointData*>(PointData);
	}

#pragma endregion

#pragma region Pool & cache

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

				if (!Reader->Bind(Source))
				{
					bInitialized = false;
					PCGEX_DELETE(Reader)
				}

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
			if (!Writer) { return; }
			
			if (AsyncManager) { PCGEX_ASYNC_WRITE_DELETE(AsyncManager, Writer); }
			else
			{
				Writer->Write();
				PCGEX_DELETE(Writer)
			}
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
			FCache<T>* Cache = GetOrCreateCache<T>(Name);
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
			PCGEx::TFAttributeReader<T>* Reader = Cache->PrepareReader();
			if (!Reader)
			{
				FWriteScopeLock WriteScopeLock(PoolLock);
				Caches.Remove(Cache);
				CacheMap.Remove(Cache->UID);
				PCGEX_DELETE(Cache)
			}
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
	
#pragma endregion 
}

namespace PCGExDataBlending
{
	static FDataBlendingOperationBase* CreateOperation(const EPCGExDataBlendingType Type, const PCGEx::FAttributeIdentity& Identity)
	{
#define PCGEX_SAO_NEW(_TYPE, _NAME, _ID) case EPCGMetadataTypes::_NAME : NewOperation = new TDataBlending##_ID<_TYPE>(); break;
#define PCGEX_BLEND_CASE(_ID) case EPCGExDataBlendingType::_ID: switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_NEW, _ID) } break;
#define PCGEX_FOREACH_BLEND(MACRO)\
PCGEX_BLEND_CASE(None)\
PCGEX_BLEND_CASE(Copy)\
PCGEX_BLEND_CASE(Average)\
PCGEX_BLEND_CASE(Weight)\
PCGEX_BLEND_CASE(WeightedSum)\
PCGEX_BLEND_CASE(Min)\
PCGEX_BLEND_CASE(Max)\
PCGEX_BLEND_CASE(Sum)\
PCGEX_BLEND_CASE(Lerp)

		FDataBlendingOperationBase* NewOperation = nullptr;

		switch (Type)
		{
		default:
		PCGEX_FOREACH_BLEND(PCGEX_BLEND_CASE)
		}

		if (NewOperation) { NewOperation->SetAttributeName(Identity.Name); }
		return NewOperation;

#undef PCGEX_SAO_NEW
#undef PCGEX_BLEND_CASE
#undef PCGEX_FOREACH_BLEND
	}
}
