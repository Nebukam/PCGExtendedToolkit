// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointIO.h"
#include "PCGExAttributeHelpers.h"
#include "PCGExDataFilter.h"
#include "PCGExGlobalSettings.h"
#include "PCGExDetails.h"
#include "Data/PCGPointData.h"
#include "Geometry/PCGExGeoPointBox.h"
#include "UObject/Object.h"
#include "PCGExHelpers.h"

#include "PCGExData.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAttributeGatherDetails : public FPCGExNameFiltersDetails
{
	GENERATED_BODY()

	FPCGExAttributeGatherDetails()
	{
		bPreservePCGExData = false;
	}

	// TODO : Expose how to handle overlaps
};

namespace PCGExData
{
	PCGEX_ASYNC_STATE(State_MergingData);

#pragma region Pool & cache

	static uint64 CacheUID(const FName FullName, const EPCGMetadataTypes Type)
	{
		return PCGEx::H64(GetTypeHash(FullName), static_cast<int32>(Type));
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FCacheBase
	{
		friend class FFacade;

	protected:
		mutable FRWLock CacheLock;
		mutable FRWLock WriteLock;
		bool bInitialized = false;
		bool bDynamicCache = false;

		int32 ReadyNum = 0;

	public:
		FName FullName = NAME_None;
		EPCGMetadataTypes Type = EPCGMetadataTypes::Unknown;
		FPCGMetadataAttributeBase* Attribute = nullptr;
		const uint64 UID;
		FPointIO* Source = nullptr;

		bool bIsPureReader = false;

		FCacheBase(const FName InFullName, const EPCGMetadataTypes InType):
			FullName(InFullName), Type(InType), UID(CacheUID(FullName, Type))
		{
		}

		virtual ~FCacheBase() = default;

		void IncrementWriteReadyNum();
		void ReadyWrite(PCGExMT::FTaskManager* AsyncManager);

		virtual void Write(PCGExMT::FTaskManager* AsyncManager);

		virtual void Fetch(const int32 StartIndex, const int32 Count)
		{
		}

		virtual bool IsDynamic() { return bDynamicCache; }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TCache : public FCacheBase
	{
	public:
		TArray<T> Values;

		T Min = T{};
		T Max = T{};

		PCGEx::TAttributeIO<T>* Reader = nullptr;
		PCGEx::TAttributeWriter<T>* Writer = nullptr;
		PCGEx::TAttributeGetter<T>* DynamicBroadcaster = nullptr;

		virtual bool IsDynamic() override { return bDynamicCache || DynamicBroadcaster; }

		TCache(const FName InFullName, const EPCGMetadataTypes InType):
			FCacheBase(InFullName, InType)
		{
		}

		virtual ~TCache() override
		{
			Flush();
		}

		PCGEx::TAttributeIO<T>* PrepareReader(const ESource InSource = ESource::In, const bool bFetch = false)
		{
			FWriteScopeLock WriteScopeLock(CacheLock);

			if (bInitialized)
			{
				if (InSource == ESource::Out && Writer) { return Writer; }
				if (Reader)
				{
#if WITH_EDITOR
					// If this throws, we're requesting a full reader on a non-dynamic one.
					// TODO : Read cache in full and toggle off dynamic.
					if (bDynamicCache) { check(bFetch) }
#endif
					return Reader;
				}
			}

			bDynamicCache = bFetch;

			if (bInitialized)
			{
				check(Writer)
				PCGEx::TAttributeReader<T>* TypedReader = new PCGEx::TAttributeReader<T>(FullName);

				if (bFetch) { if (!TypedReader->BindForFetch(Source)) { PCGEX_DELETE(TypedReader) } }
				else { if (!TypedReader->Bind(Source)) { PCGEX_DELETE(TypedReader) } }

				Attribute = TypedReader->Accessor->GetAttribute();
				Reader = TypedReader;
				bIsPureReader = false;

				return Reader;
			}

			bInitialized = true;

			PCGEx::TAttributeReader<T>* TypedReader = new PCGEx::TAttributeReader<T>(FullName);

			if (bFetch) { if (!TypedReader->BindForFetch(Source)) { PCGEX_DELETE(TypedReader) } }
			else { if (!TypedReader->Bind(Source)) { PCGEX_DELETE(TypedReader) } }

			bInitialized = TypedReader ? true : false;
			Attribute = TypedReader ? TypedReader->Accessor->GetAttribute() : nullptr;
			bIsPureReader = true;

			Reader = TypedReader;
			return Reader;
		}

		PCGEx::TAttributeWriter<T>* PrepareWriter(T DefaultValue, bool bAllowInterpolation, const bool bUninitialized = false)
		{
			FWriteScopeLock WriteScopeLock(CacheLock);

			if (bInitialized) { return Writer; } // TODO : Handle cases where we already have a reader

			bInitialized = true;
			bIsPureReader = false;

			Writer = new PCGEx::TAttributeWriter<T>(FullName, DefaultValue, bAllowInterpolation);

			if (bUninitialized && !PCGEx::RequireInit(Type)) { Writer->BindAndSetNumUninitialized(Source); }
			else { Writer->BindAndGet(Source); }
			Attribute = Writer->Accessor->GetAttribute();

			return Writer;
		}

		PCGEx::TAttributeWriter<T>* PrepareWriter(const bool bUninitialized = false)
		{
			{
				FWriteScopeLock WriteScopeLock(CacheLock);
				if (bInitialized) { return Writer; } // TODO : Handle cases where we already have a reader
			}

			if (Source->GetIn())
			{
				if (const FPCGMetadataAttribute<T>* ExistingAttribute = Source->GetIn()->Metadata->GetConstTypedAttribute<T>(FullName))
				{
					return PrepareWriter(
						ExistingAttribute->GetValue(PCGDefaultValueKey),
						ExistingAttribute->AllowsInterpolation(),
						bUninitialized);
				}
			}

			{
				FWriteScopeLock WriteScopeLock(CacheLock);
				bInitialized = true;
				bIsPureReader = false;

				Writer = new PCGEx::TAttributeWriter<T>(FullName);

				if (bUninitialized && PCGEx::RequireInit(Type)) { Writer->BindAndSetNumUninitialized(Source); }
				else { Writer->BindAndGet(Source); }
				Attribute = Writer->Accessor->GetAttribute();

				return Writer;
			}
		}

		void Grab(PCGEx::TAttributeGetter<T>* Getter, bool bCaptureMinMax = false)
		{
			FWriteScopeLock WriteScopeLock(CacheLock);

			if (bInitialized)
			{
#if WITH_EDITOR
				check(!DynamicBroadcaster)
#endif
				return;
			}

			bInitialized = true;
			bIsPureReader = true;

			PCGEX_SET_NUM_UNINITIALIZED(Values, Source->GetNum(ESource::In))

			Getter->GrabAndDump(Source, Values, bCaptureMinMax, Min, Max);
			Attribute = Getter->Attribute;
		}

		void SetDynamicGetter(PCGEx::TAttributeGetter<T>* Getter)
		{
			FWriteScopeLock WriteScopeLock(CacheLock);

			if (bInitialized) { return; }

			bInitialized = true;
			bIsPureReader = true;

			bDynamicCache = true;
			DynamicBroadcaster = Getter;
			Attribute = Getter->Attribute;

			PCGEX_SET_NUM_UNINITIALIZED(Values, Source->GetNum())
		}

		virtual void Write(PCGExMT::FTaskManager* AsyncManager) override
		{
			if (!Writer) { return; }

			if (AsyncManager && !GetDefault<UPCGExGlobalSettings>()->IsSmallPointSize(Source->GetNum(ESource::Out)))
			{
				PCGEX_ASYNC_WRITE_DELETE(AsyncManager, Writer);
			}
			else
			{
				Writer->Write();
				PCGEX_DELETE(Writer)
			}
		}

		virtual void Fetch(const int32 StartIndex, const int32 Count) override
		{
			if (!IsDynamic()) { return; }
			if (DynamicBroadcaster) { DynamicBroadcaster->Fetch(Source, Values, StartIndex, Count); }
			else if (Reader) { Reader->Fetch(StartIndex, Count); }
		}

		void Flush()
		{
			bInitialized = false;
			Values.Empty();
			PCGEX_DELETE(Reader)
			PCGEX_DELETE(Writer)
			PCGEX_DELETE(DynamicBroadcaster)
		}
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FFacade
	{
		mutable FRWLock PoolLock;
		mutable FRWLock CloudLock;

	public:
		FPointIO* Source = nullptr;
		TArray<FCacheBase*> Caches;
		TMap<uint64, FCacheBase*> CacheMap;
		PCGExGeo::FPointBoxCloud* Cloud = nullptr;

		bool bSupportsDynamic = false;

		FCacheBase* FindCache(const uint64 UID);

		explicit FFacade(FPointIO* InSource):
			Source(InSource)
		{
		}

		bool ShareSource(const FFacade* OtherManager) const { return this == OtherManager || OtherManager->Source == Source; }

		template <typename T>
		TCache<T>* FindCache(const FName FullName)
		{
			FCacheBase* Found = FindCache(CacheUID(FullName, PCGEx::GetMetadataType(T{})));
			if (!Found) { return nullptr; }
			return static_cast<TCache<T>*>(Found);
		}

		template <typename T>
		TCache<T>* GetCache(FName FullName)
		{
			TCache<T>* NewCache = FindCache<T>(FullName);
			if (NewCache) { return NewCache; }

			{
				FWriteScopeLock WriteScopeLock(PoolLock);
				NewCache = new TCache<T>(FullName, PCGEx::GetMetadataType(T{}));
				NewCache->Source = Source;
				Caches.Add(NewCache);
				CacheMap.Add(NewCache->UID, NewCache);
				return NewCache;
			}
		}

		template <typename T>
		TCache<T>* GetBroadcaster(const FPCGAttributePropertyInputSelector& InSelector, bool bCaptureMinMax = false)
		{
			PCGEx::TAttributeGetter<T>* Getter;

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
			case EPCGMetadataTypes::Name:
			case EPCGMetadataTypes::Count:
			case EPCGMetadataTypes::Unknown:
				return nullptr;
			// TODO : Proper implementation, this is cursed
			case EPCGMetadataTypes::Double:
				Getter = static_cast<PCGEx::TAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalSingleFieldGetter()));
				break;
			case EPCGMetadataTypes::Integer32:
				Getter = static_cast<PCGEx::TAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalIntegerGetter()));
				break;
			case EPCGMetadataTypes::Vector:
				Getter = static_cast<PCGEx::TAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalVectorGetter()));
				break;
			case EPCGMetadataTypes::Boolean:
				Getter = static_cast<PCGEx::TAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalBoolGetter()));
				break;
			case EPCGMetadataTypes::String:
				Getter = static_cast<PCGEx::TAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalToStringGetter()));
				break;
			}

			Getter->Capture(InSelector);
			if (!Getter->SoftGrab(Source))
			{
				PCGEX_DELETE(Getter)
				return nullptr;
			}

			TCache<T>* Cache = GetCache<T>(Getter->FullName);
			Cache->Grab(Getter, bCaptureMinMax);
			PCGEX_DELETE(Getter)

			return Cache;
		}

		template <typename T>
		TCache<T>* GetScopedBroadcaster(const FPCGAttributePropertyInputSelector& InSelector)
		{
			if (!bSupportsDynamic) { return GetBroadcaster<T>(InSelector); }

			PCGEx::TAttributeGetter<T>* Getter;

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
			case EPCGMetadataTypes::Name:
			case EPCGMetadataTypes::Count:
			case EPCGMetadataTypes::Unknown:
				return nullptr;
			// TODO : Proper implementation, this is cursed
			case EPCGMetadataTypes::Double:
				Getter = static_cast<PCGEx::TAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalSingleFieldGetter()));
				break;
			case EPCGMetadataTypes::Integer32:
				Getter = static_cast<PCGEx::TAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalIntegerGetter()));
				break;
			case EPCGMetadataTypes::Vector:
				Getter = static_cast<PCGEx::TAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalVectorGetter()));
				break;
			case EPCGMetadataTypes::Boolean:
				Getter = static_cast<PCGEx::TAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalBoolGetter()));
				break;
			case EPCGMetadataTypes::String:
				Getter = static_cast<PCGEx::TAttributeGetter<T>*>(static_cast<PCGEx::FAttributeGetterBase*>(new PCGEx::FLocalToStringGetter()));
				break;
			}

			Getter->Capture(InSelector);
			if (!Getter->InitForFetch(Source))
			{
				PCGEX_DELETE(Getter)
				return nullptr;
			}

			TCache<T>* Cache = GetCache<T>(Getter->FullName);
			Cache->SetDynamicGetter(Getter);

			return Cache;
		}

		template <typename T>
		PCGEx::TAttributeWriter<T>* GetWriter(const FPCGMetadataAttribute<T>* InAttribute, bool bUninitialized)
		{
			TCache<T>* Cache = GetCache<T>(InAttribute->Name);
			PCGEx::TAttributeWriter<T>* Writer = Cache->PrepareWriter(InAttribute->GetValue(PCGDefaultValueKey), InAttribute->AllowsInterpolation(), bUninitialized);
			return Writer;
		}

		template <typename T>
		PCGEx::TAttributeWriter<T>* GetWriter(const FName InName, T DefaultValue, bool bAllowInterpolation, bool bUninitialized)
		{
			TCache<T>* Cache = GetCache<T>(InName);
			PCGEx::TAttributeWriter<T>* Writer = Cache->PrepareWriter(DefaultValue, bAllowInterpolation, bUninitialized);
			return Writer;
		}

		template <typename T>
		PCGEx::TAttributeWriter<T>* GetWriter(const FName InName, bool bUninitialized)
		{
			TCache<T>* Cache = GetCache<T>(InName);
			PCGEx::TAttributeWriter<T>* Writer = Cache->PrepareWriter(bUninitialized);
			return Writer;
		}

		template <typename T>
		PCGEx::TAttributeIO<T>* GetReader(const FName InName, const ESource InSource = ESource::In)
		{
			TCache<T>* Cache = GetCache<T>(InName);
			PCGEx::TAttributeIO<T>* Reader = Cache->PrepareReader(InSource, false);

			if (!Reader)
			{
				FWriteScopeLock WriteScopeLock(PoolLock);
				Caches.Remove(Cache);
				CacheMap.Remove(Cache->UID);
				PCGEX_DELETE(Cache)
			}

			return Reader;
		}

		template <typename T>
		PCGEx::TAttributeIO<T>* GetScopedReader(const FName InName)
		{
			if (!bSupportsDynamic) { return GetReader<T>(InName); }

			TCache<T>* Cache = GetCache<T>(InName);
			PCGEx::TAttributeIO<T>* Reader = Cache->PrepareReader(ESource::In, true);

			if (!Reader)
			{
				FWriteScopeLock WriteScopeLock(PoolLock);
				Caches.Remove(Cache);
				CacheMap.Remove(Cache->UID);
				PCGEX_DELETE(Cache)
			}

			return Reader;
		}

		FPCGMetadataAttributeBase* FindMutableAttribute(const FName InName, const ESource InSource = ESource::In) const
		{
			const UPCGPointData* Data = Source->GetData(InSource);
			if (!Data) { return nullptr; }
			return Data->Metadata->GetMutableAttribute(InName);
		}

		const FPCGMetadataAttributeBase* FindConstAttribute(const FName InName, const ESource InSource = ESource::In) const
		{
			const UPCGPointData* Data = Source->GetData(InSource);
			if (!Data) { return nullptr; }
			return Data->Metadata->GetConstAttribute(InName);
		}

		template <typename T>
		FPCGMetadataAttribute<T>* FindMutableAttribute(const FName InName, const ESource InSource = ESource::In) const
		{
			const UPCGPointData* Data = Source->GetData(InSource);
			if (!Data) { return nullptr; }
			return Data->Metadata->GetMutableTypedAttribute<T>(InName);
		}

		template <typename T>
		const FPCGMetadataAttribute<T>* FindConstAttribute(const FName InName, const ESource InSource = ESource::In) const
		{
			const UPCGPointData* Data = Source->GetData(InSource);
			if (!Data) { return nullptr; }
			return Data->Metadata->GetConstTypedAttribute<T>(InName);
		}

		PCGExGeo::FPointBoxCloud* GetCloud(const EPCGExPointBoundsSource BoundsSource, const double Epsilon = DBL_EPSILON)
		{
			FWriteScopeLock WriteScopeLock(CloudLock);

			if (Cloud) { return Cloud; }

			Cloud = new PCGExGeo::FPointBoxCloud(GetIn(), BoundsSource, Epsilon);
			return Cloud;
		}

		const UPCGPointData* GetData(const ESource InSource) const { return Source->GetData(InSource); }
		const UPCGPointData* GetIn() const { return Source->GetIn(); }
		UPCGPointData* GetOut() const { return Source->GetOut(); }

		~FFacade()
		{
			Flush();
			Source = nullptr;
			PCGEX_DELETE(Cloud)
		}

		void Flush()
		{
			PCGEX_DELETE_TARRAY(Caches)
			CacheMap.Empty();
		}

		void Write(PCGExMT::FTaskManager* AsyncManager, const bool bFlush)
		{
			for (FCacheBase* Cache : Caches) { Cache->Write(AsyncManager); }
			if (bFlush) { Flush(); }
		}

		void Fetch(const int32 StartIndex, const int32 Count) { for (FCacheBase* Cache : Caches) { Cache->Fetch(StartIndex, Count); } }
		void Fetch(const uint64 Scope) { Fetch(PCGEx::H64A(Scope), PCGEx::H64B(Scope)); }
	};

	static void GetCollectionFacades(const FPointIOCollection* InCollection, TArray<FFacade*>& OutFacades)
	{
		OutFacades.Empty();
		PCGEX_SET_NUM_UNINITIALIZED(OutFacades, InCollection->Num())
		for (int i = 0; OutFacades.Num(); i++) { OutFacades[i] = new FFacade(InCollection->Pairs[i]); }
	}

#pragma endregion

#pragma region Compound

	struct /*PCGEXTENDEDTOOLKIT_API*/ FIdxCompound
	{
	protected:
		mutable FRWLock CompoundLock;

	public:
		TSet<int32> IOIndices;
		TSet<uint64> CompoundedHashSet;

		FIdxCompound() { CompoundedHashSet.Empty(); }

		~FIdxCompound()
		{
			IOIndices.Empty();
			CompoundedHashSet.Empty();
		}

		int32 Num() const { return CompoundedHashSet.Num(); }

		void ComputeWeights(
			const TArray<FFacade*>& Sources,
			const TMap<uint32, int32>& SourcesIdx,
			const FPCGPoint& Target,
			const FPCGExDistanceDetails& InDistanceDetails,
			TArray<int32>& OutIOIdx,
			TArray<int32>& OutPointsIdx,
			TArray<double>& OutWeights) const;

		uint64 Add(const int32 IOIndex, const int32 PointIndex);

		void Clear()
		{
			IOIndices.Empty();
			CompoundedHashSet.Empty();
		}
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FIdxCompoundList
	{
		TArray<FIdxCompound*> Compounds;

		FIdxCompoundList() { Compounds.Empty(); }
		~FIdxCompoundList() { PCGEX_DELETE_TARRAY(Compounds) }

		int32 Num() const { return Compounds.Num(); }

		FORCEINLINE FIdxCompound* New(const int32 IOIndex, const int32 PointIndex)
		{
			FIdxCompound* NewPointCompound = new FIdxCompound();
			Compounds.Add(NewPointCompound);

			NewPointCompound->IOIndices.Add(IOIndex);
			const uint64 H = PCGEx::H64(IOIndex, PointIndex);
			NewPointCompound->CompoundedHashSet.Add(H);

			return NewPointCompound;
		}

		FORCEINLINE uint64 Add(const int32 Index, const int32 IOIndex, const int32 PointIndex) { return Compounds[Index]->Add(IOIndex, PointIndex); }
		FORCEINLINE bool IOIndexOverlap(const int32 InIdx, const TSet<int32>& InIndices)
		{
			const TSet<int32> Overlap = Compounds[InIdx]->IOIndices.Intersect(InIndices);
			return Overlap.Num() > 0;
		}

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
	static bool TryReadMark(const FPointIO* PointIO, const FName MarkID, T& OutMark)
	{
		return TryReadMark(PointIO->GetIn() ? PointIO->GetIn()->Metadata : PointIO->GetOut()->Metadata, MarkID, OutMark);
	}

	static void WriteId(const FPointIO& PointIO, const FName IdName, const int64 Id)
	{
		FString OutId;
		PointIO.Tags->Add(IdName.ToString(), Id, OutId);
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
}
