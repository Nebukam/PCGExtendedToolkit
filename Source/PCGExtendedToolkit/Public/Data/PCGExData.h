// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointIO.h"
#include "PCGExAttributeHelpers.h"
#include "PCGExGlobalSettings.h"
#include "PCGExSettings.h"
#include "Data/PCGPointData.h"
#include "UObject/Object.h"
#include "PCGExData.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExForwardSettings
{
	GENERATED_BODY()

	FPCGExForwardSettings()
	{
	}

	/** Is forwarding enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bEnabled = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bEnabled"))
	EPCGExAttributeFilter FilterAttributes = EPCGExAttributeFilter::Include;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bEnabled && FilterAttributes!=EPCGExAttributeFilter::All", EditConditionHides))
	TArray<FName> FilteredAttributes;

	bool CanProcess(const FName AttributeName) const
	{
		switch (FilterAttributes)
		{
		default: ;
		case EPCGExAttributeFilter::All:
			return true;
		case EPCGExAttributeFilter::Exclude:
			return !FilteredAttributes.Contains(AttributeName);
		case EPCGExAttributeFilter::Include:
			return FilteredAttributes.Contains(AttributeName);
		}
	}

	void Filter(TArray<PCGEx::FAttributeIdentity>& Identities) const
	{
		if (FilterAttributes == EPCGExAttributeFilter::All) { return; }
		for (int i = 0; i < Identities.Num(); i++)
		{
			if (!CanProcess(Identities[i].Name))
			{
				Identities.RemoveAt(i);
				i--;
			}
		}
	}
};

namespace PCGExData
{
	PCGEX_ASYNC_STATE(State_MergingData);

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
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FCache : public FCacheBase
	{
	public:
		TArray<T> Values;

		T Min = T{};
		T Max = T{};

		PCGEx::FAttributeIOBase<T>* Reader = nullptr;
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

		PCGEx::FAttributeIOBase<T>* PrepareReader(const ESource InSource = ESource::In)
		{
			{
				FReadScopeLock ReadScopeLock(CacheLock);
				if (bInitialized)
				{
					if (InSource == ESource::Out && Writer) { return Writer; }
					if (Reader) { return Reader; }
				}
			}

			if (bInitialized)
			{
				check(Writer)
				FWriteScopeLock WriteScopeLock(CacheLock);
				PCGEx::TFAttributeReader<T>* TypedReader = new PCGEx::TFAttributeReader<T>(FullName);
				if (!TypedReader->Bind(Source)) { PCGEX_DELETE(TypedReader) }
				else { Attribute = TypedReader->Accessor->GetAttribute(); }
				Reader = TypedReader;
				bIsPureReader = false;
				return Reader;
			}
			FWriteScopeLock WriteScopeLock(CacheLock);
			bInitialized = true;

			PCGEx::TFAttributeReader<T>* TypedReader = new PCGEx::TFAttributeReader<T>(FullName);

			if (!TypedReader->Bind(Source))
			{
				bInitialized = false;
				PCGEX_DELETE(TypedReader)
			}
			else
			{
				Attribute = TypedReader->Accessor->GetAttribute();
				bIsPureReader = true;
			}

			Reader = TypedReader;
			return Reader;
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
				bIsPureReader = false;

				Writer = new PCGEx::TFAttributeWriter<T>(FullName, DefaultValue, bAllowInterpolation);

				if (bUninitialized) { Writer->BindAndSetNumUninitialized(Source); }
				else { Writer->BindAndGet(Source); }
				Attribute = Writer->Accessor->GetAttribute();

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
				bIsPureReader = false;

				Writer = new PCGEx::TFAttributeWriter<T>(FullName);

				if (bUninitialized) { Writer->BindAndSetNumUninitialized(Source); }
				else { Writer->BindAndGet(Source); }
				Attribute = Writer->Accessor->GetAttribute();

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
				bIsPureReader = true;

				Getter->GrabAndDump(Source, Values, bCaptureMinMax, Min, Max);
				Attribute = Getter->Attribute;
			}
		}

		virtual void Write(PCGExMT::FTaskManager* AsyncManager) override
		{
			if (!Writer) { return; }

			if (AsyncManager && !GetDefault<UPCGExGlobalSettings>()->IsSmallPointSize(Source->GetOutNum()))
			{
				PCGEX_ASYNC_WRITE_DELETE(AsyncManager, Writer);
			}
			else
			{
				Writer->Write();
				PCGEX_DELETE(Writer)
			}
		}
	};

	class PCGEXTENDEDTOOLKIT_API FFacade
	{
		mutable FRWLock PoolLock;

	public:
		FPointIO* Source = nullptr;
		TArray<FCacheBase*> Caches;
		TMap<uint64, FCacheBase*> CacheMap;

		FCacheBase* TryGetCache(const uint64 UID);

		explicit FFacade(FPointIO* InSource):
			Source(InSource)
		{
		}

		bool ShareSource(const FFacade* OtherManager) const { return this == OtherManager || OtherManager->Source == Source; }

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
		FCache<T>* GetOrCreateGetter(const FPCGAttributePropertyInputSelector& InSelector, bool bCaptureMinMax = false)
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

			Getter->Capture(InSelector);
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
		PCGEx::TFAttributeWriter<T>* GetOrCreateWriter(const FPCGMetadataAttribute<T>* InAttribute, bool bUninitialized)
		{
			FCache<T>* Cache = GetOrCreateCache<T>(InAttribute->Name);
			PCGEx::TFAttributeWriter<T>* Writer = Cache->PrepareWriter(InAttribute->GetValueFromItemKey(PCGInvalidEntryKey), InAttribute->AllowsInterpolation(), bUninitialized);
			return Writer;
		}

		template <typename T>
		PCGEx::TFAttributeWriter<T>* GetOrCreateWriter(const FName InName, T DefaultValue, bool bAllowInterpolation, bool bUninitialized)
		{
			FCache<T>* Cache = GetOrCreateCache<T>(InName);
			PCGEx::TFAttributeWriter<T>* Writer = Cache->PrepareWriter(DefaultValue, bAllowInterpolation, bUninitialized);
			return Writer;
		}

		template <typename T>
		PCGEx::TFAttributeWriter<T>* GetOrCreateWriter(const FName InName, bool bUninitialized)
		{
			FCache<T>* Cache = GetOrCreateCache<T>(InName);
			PCGEx::TFAttributeWriter<T>* Writer = Cache->PrepareWriter(bUninitialized);
			return Writer;
		}

		template <typename T>
		PCGEx::FAttributeIOBase<T>* GetOrCreateReader(const FName InName, const ESource InSource = ESource::In)
		{
			FCache<T>* Cache = GetOrCreateCache<T>(InName);
			PCGEx::FAttributeIOBase<T>* Reader = Cache->PrepareReader(InSource);

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

		const UPCGPointData* GetData(ESource InSource) const { return Source->GetData(InSource); }
		const UPCGPointData* GetIn() const { return Source->GetIn(); }
		UPCGPointData* GetOut() const { return Source->GetOut(); }

		~FFacade()
		{
			Flush();
			Source = nullptr;
		}

		void Flush()
		{
			PCGEX_DELETE_TARRAY(Caches)
			CacheMap.Empty();
		}

		void Write(PCGExMT::FTaskManager* AsyncManager, bool bFlush)
		{
			for (FCacheBase* Cache : Caches) { Cache->Write(AsyncManager); }
			if (bFlush) { Flush(); }
		}
	};

	static void GetCollectionFacades(const FPointIOCollection* InCollection, TArray<FFacade*>& OutFacades)
	{
		OutFacades.Empty();
		OutFacades.SetNumUninitialized(InCollection->Num());
		for (int i = 0; OutFacades.Num(); i++) { OutFacades[i] = new FFacade(InCollection->Pairs[i]); }
	}

#pragma endregion

#pragma region Compound

	struct PCGEXTENDEDTOOLKIT_API FIdxCompound
	{
		TSet<int32> IOIndices;
		TSet<uint64> CompoundedHashSet;

		FIdxCompound() { CompoundedHashSet.Empty(); }

		~FIdxCompound()
		{
			IOIndices.Empty();
			CompoundedHashSet.Empty();
		}

		int32 Num() const { return CompoundedHashSet.Num(); }

		void ComputeWeights(const TArray<FFacade*>& Sources, const TMap<uint32, int32>& SourcesIdx, const FPCGPoint& Target, const FPCGExDistanceSettings& DistSettings, TArray<uint64>& OutCompoundHashes, TArray<double>& OutWeights);

		uint64 Add(const int32 IOIndex, const int32 PointIndex);
	};

	struct PCGEXTENDEDTOOLKIT_API FIdxCompoundList
	{
		TArray<FIdxCompound*> Compounds;

		FIdxCompoundList() { Compounds.Empty(); }
		~FIdxCompoundList() { PCGEX_DELETE_TARRAY(Compounds) }

		int32 Num() const { return Compounds.Num(); }

		FIdxCompound* New();

		FORCEINLINE uint64 Add(const int32 Index, const int32 IOIndex, const int32 PointIndex);
		FORCEINLINE bool IOIndexOverlap(const int32 InIdx, const TSet<int32>& InIndices);

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


	class PCGEXTENDEDTOOLKIT_API FDataForwardHandler
	{
		const FPCGExForwardSettings* Settings = nullptr;
		const FPointIO* SourceIO = nullptr;
		TArray<PCGEx::FAttributeIdentity> Identities;

	public:
		~FDataForwardHandler();
		explicit FDataForwardHandler(const FPCGExForwardSettings* InSettings, const FPointIO* InSourceIO);
		void Forward(int32 SourceIndex, const FPointIO* Target);
	};
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeToTagSettings
{
	GENERATED_BODY()

	FPCGExAttributeToTagSettings()
	{
	}

	/** Use attribute value to tag output data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bEnabled = false;

	/** Attributes which value will be used as tags. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bEnabled"))
	TArray<FPCGAttributePropertyInputSelector> Attributes;

	PCGExData::FFacade* TagSource = nullptr;
	TArray<PCGEx::FLocalToStringGetter*> Getters;

	bool Init(const FPCGContext* InContext, const PCGExData::FFacade* TagSourceFacade)
	{
		if (!bEnabled) { return true; }
		for (FPCGAttributePropertyInputSelector& Selector : Attributes)
		{
			PCGEx::FLocalToStringGetter* Getter = new PCGEx::FLocalToStringGetter();
			Getter->Capture(Selector);
			if (!Getter->SoftGrab(TagSourceFacade->Source))
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Missing specified Tag attribute."));
				Cleanup();
				return false;
			}
			Getters.Add(Getter);
		}

		return true;
	}

	void Tag(const FPCGPoint& Point, const PCGExData::FPointIO* PointIO) const
	{
		if (!bEnabled) { return; }
		for (PCGEx::FLocalToStringGetter* Getter : Getters)
		{
			FString Tag = Getter->SoftGet(Point, TEXT(""));
			if (Tag.IsEmpty()) { continue; }
			PointIO->Tags->RawTags.Add(Tag);
		}
	}

	void Tag(const int32 TagIndex, const PCGExData::FPointIO* PointIO) const
	{
		Tag(TagSource->GetIn()->GetPoint(TagIndex), PointIO);
	}

	void Cleanup()
	{
		PCGEX_DELETE_TARRAY(Getters);
	}
};

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
