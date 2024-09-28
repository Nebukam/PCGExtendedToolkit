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

#pragma region Pool & Buffers

	static uint64 BufferUID(const FName FullName, const EPCGMetadataTypes Type)
	{
		return PCGEx::H64(GetTypeHash(FullName), static_cast<int32>(Type));
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FBufferBase
	{
		friend class FFacade;

	protected:
		mutable FRWLock BufferLock;
		mutable FRWLock WriteLock;

		bool bScopedBuffer = false;

		TArrayView<const FPCGPoint> InPoints;
		TArrayView<FPCGPoint> OutPoints;

	public:
		FName FullName = NAME_None;
		EPCGMetadataTypes Type = EPCGMetadataTypes::Unknown;

		const FPCGMetadataAttributeBase* InAttribute = nullptr;
		FPCGMetadataAttributeBase* OutAttribute = nullptr;

		int32 BufferIndex = -1;
		const uint64 UID;
		const TSharedRef<FPointIO> Source;


		FBufferBase(const TSharedRef<FPointIO>& InSource, const FName InFullName, const EPCGMetadataTypes InType):
			FullName(InFullName), Type(InType), UID(BufferUID(FullName, Type)), Source(InSource)
		{
			PCGEX_LOG_CTR(FBufferBase)
		}

		virtual ~FBufferBase()
		{
			PCGEX_LOG_DTR(FBufferBase)
		}

		virtual void Write()
		{
		}

		virtual void Fetch(const int32 StartIndex, const int32 Count)
		{
		}

		virtual bool IsScoped() { return bScopedBuffer; }
		virtual bool IsWritable() { return false; }
		virtual bool IsReadable() { return false; }

		FORCEINLINE bool GetAllowsInterpolation() const { return OutAttribute ? OutAttribute->AllowsInterpolation() : InAttribute ? InAttribute->AllowsInterpolation() : false; }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TBuffer : public FBufferBase
	{
	protected:
		TUniquePtr<FPCGAttributeAccessor<T>> InAccessor;
		const FPCGMetadataAttribute<T>* TypedInAttribute = nullptr;

		FPCGMetadataAttribute<T>* TypedOutAttribute = nullptr;

		TSharedPtr<TArray<T>> InValues;
		TSharedPtr<TArray<T>> OutValues;

	public:
		T Min = T{};
		T Max = T{};

		TSharedPtr<PCGEx::TAttributeGetter<T>> ScopedBroadcaster;

		virtual bool IsScoped() override { return bScopedBuffer || ScopedBroadcaster; }

		TBuffer(const TSharedRef<FPointIO>& InSource, const FName InFullName, const EPCGMetadataTypes InType):
			FBufferBase(InSource, InFullName, InType)
		{
		}

		virtual ~TBuffer() override
		{
			Flush();
		}

		virtual bool IsWritable() override { return OutValues ? true : false; }
		virtual bool IsReadable() override { return InValues ? true : false; }

		TSharedPtr<TArray<T>> GetInValues() { return InValues; }
		TSharedPtr<TArray<T>> GetOutValues() { return OutValues; }
		const FPCGMetadataAttribute<T>* GetTypedInAttribute() const { return TypedInAttribute; }
		FPCGMetadataAttribute<T>* GetTypedOutAttribute() { return TypedOutAttribute; }

		FORCEINLINE T& GetMutable(const int32 Index) { return *(OutValues->GetData() + Index); }
		FORCEINLINE const T& GetConst(const int32 Index) { return *(OutValues->GetData() + Index); }
		FORCEINLINE const T& Read(const int32 Index) const { return *(InValues->GetData() + Index); }
		FORCEINLINE const T& ReadImmediate(const int32 Index) const { return TypedInAttribute->GetValueFromItemKey(InPoints[Index]); }

		FORCEINLINE void Set(const int32 Index, const T& Value) { *(OutValues->GetData() + Index) = Value; }
		FORCEINLINE void SetImmediate(const int32 Index, const T& Value) { TypedOutAttribute->SetValue(InPoints[Index], Value); }

		void PrepareForRead(const bool bScoped, const FPCGMetadataAttributeBase* Attribute)
		{
			if (InValues) { return; }

			const TArray<FPCGPoint>& InPts = Source->GetIn()->GetPoints();
			const int32 NumPoints = InPts.Num();
			InPoints = MakeArrayView(InPts.GetData(), NumPoints);

			InValues = MakeShared<TArray<T>>();
			PCGEx::InitArray(InValues, NumPoints);

			InAttribute = Attribute;
			TypedInAttribute = Attribute ? static_cast<const FPCGMetadataAttribute<T>*>(Attribute) : nullptr;

			bScopedBuffer = bScoped;
		}

		void PrepareForWrite(FPCGMetadataAttributeBase* Attribute, const bool bUninitialized, const T& InDefaultValue)
		{
			if (OutValues) { return; }

			TArray<FPCGPoint>& OutPts = Source->GetOut()->GetMutablePoints();
			const int32 NumPoints = OutPts.Num();
			OutPoints = MakeArrayView(OutPts.GetData(), NumPoints);

			OutValues = MakeShared<TArray<T>>();
			if (bUninitialized) { PCGEx::InitArray(OutValues, NumPoints); }
			else { OutValues->Init(InDefaultValue, NumPoints); }

			OutAttribute = Attribute;
			TypedOutAttribute = Attribute ? static_cast<FPCGMetadataAttribute<T>*>(Attribute) : nullptr;
		}

		bool PrepareRead(const ESource InSource = ESource::In, const bool bScoped = false)
		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			if (InValues)
			{
				if (bScopedBuffer && !bScoped)
				{
					// Un-scoping reader.
					Fetch(0, InValues->Num());
					bScopedBuffer = false;
				}

				if (InSource == ESource::In && OutValues && InValues == OutValues)
				{
					// Out-source Reader was created before writer, this is bad?
					InValues = nullptr;
				}
				else
				{
					return true;
				}
			}

			if (InSource == ESource::Out)
			{
				// Reading from output
				check(OutValues)
				InValues = OutValues;
				return true;
			}

			UPCGMetadata* InMetadata = Source->GetIn()->Metadata;
			TypedInAttribute = InMetadata->GetConstTypedAttribute<T>(FullName);
			InAccessor = MakeUnique<FPCGAttributeAccessor<T>>(TypedInAttribute, InMetadata);

			if (!TypedInAttribute || !InAccessor.IsValid())
			{
				TypedInAttribute = nullptr;
				InAccessor = nullptr;
				return false;
			}

			PrepareForRead(bScoped, TypedInAttribute);

			if (!bScopedBuffer)
			{
				TArrayView<T> InRange = MakeArrayView(InValues->GetData(), InValues->Num());
				InAccessor->GetRange(InRange, 0, *Source->CreateInKeys());
			}

			return true;
		}

		bool PrepareWrite(const T& DefaultValue, bool bAllowInterpolation, const bool bUninitialized = false)
		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			if (OutValues) { return true; }

			UPCGMetadata* OutMetadata = Source->GetOut()->Metadata;
			TypedOutAttribute = OutMetadata->FindOrCreateAttribute(FullName, DefaultValue, bAllowInterpolation);
			TUniquePtr<FPCGAttributeAccessor<T>> OutAccessor = MakeUnique<FPCGAttributeAccessor<T>>(TypedOutAttribute, OutMetadata);

			if (!TypedOutAttribute || !OutAccessor.IsValid())
			{
				TypedOutAttribute = nullptr;
				return false;
			}

			PrepareForWrite(TypedOutAttribute, bUninitialized, DefaultValue);

			if (!bUninitialized)
			{
				if (Source->GetIn() && Source->GetIn()->Metadata->GetConstTypedAttribute<T>(FullName))
				{
					// TODO : Scoped get would be better here
					// Get existing values
					TArrayView<T> OutRange = MakeArrayView(OutValues->GetData(), OutValues->Num());
					OutAccessor->GetRange(OutRange, 0, *Source->CreateOutKeys());
				}
			}

			return true;
		}

		bool PrepareWrite(const bool bUninitialized = false)
		{
			{
				FWriteScopeLock WriteScopeLock(BufferLock);
				if (OutValues) { return true; }
			}

			if (Source->GetIn())
			{
				if (const FPCGMetadataAttribute<T>* ExistingAttribute = Source->GetIn()->Metadata->GetConstTypedAttribute<T>(FullName))
				{
					return PrepareWrite(
						ExistingAttribute->GetValue(PCGDefaultValueKey),
						ExistingAttribute->AllowsInterpolation(),
						bUninitialized);
				}
			}

			return PrepareWrite(T{}, true, bUninitialized);
		}

		void SetScopedGetter(const TSharedRef<PCGEx::TAttributeGetter<T>>& Getter)
		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			PrepareForRead(true, Getter->GetAttribute());
			ScopedBroadcaster = Getter;

			PCGEx::InitArray(InValues, Source->GetNum());
		}

		virtual void Write() override
		{
			if (!IsWritable()) { return; }

			TArrayView<const T> View = MakeArrayView(OutValues->GetData(), OutValues->Num());
			TUniquePtr<FPCGAttributeAccessor<T>> OutAccessor = MakeUnique<FPCGAttributeAccessor<T>>(TypedOutAttribute, Source->GetOut()->Metadata);

			OutAccessor->SetRange(View, 0, *Source->CreateOutKeys());
		}

		virtual void Fetch(const int32 StartIndex, const int32 Count) override
		{
			if (!IsScoped()) { return; }
			if (ScopedBroadcaster) { ScopedBroadcaster->Fetch(*InValues, StartIndex, Count); }
			if (InAccessor.IsValid())
			{
				TArrayView<T> ReadRange = MakeArrayView(InValues->GetData() + StartIndex, Count);
				InAccessor->GetRange(ReadRange, StartIndex, *Source->CreateInKeys());
			}

			//if (OutAccessor.IsValid())
			//{
			//	TArrayView<T> WriteRange = MakeArrayView(OutValues->GetData() + StartIndex, Count);
			//	OutAccessor->GetRange(WriteRange, StartIndex, *InKeys);
			//}
		}

		void Flush()
		{
			InValues.Reset();
			OutValues.Reset();
			ScopedBroadcaster.Reset();
		}
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FFacade
	{
		mutable FRWLock PoolLock;
		mutable FRWLock CloudLock;

	public:
		TSharedRef<FPointIO> Source;
		TArray<TSharedPtr<FBufferBase>> Buffers;
		TMap<uint64, TSharedPtr<FBufferBase>> BufferMap;
		TSharedPtr<PCGExGeo::FPointBoxCloud> Cloud;

		bool bSupportsScopedGet = false;

		FORCEINLINE int32 GetNum(const ESource InSource = ESource::In) const { return Source->GetNum(InSource); }

		TSharedPtr<FBufferBase> FindBufferUnsafe(const uint64 UID);
		TSharedPtr<FBufferBase> FindBuffer(const uint64 UID);

		explicit FFacade(const TSharedRef<FPointIO>& InSource):
			Source(InSource)
		{
			PCGEX_LOG_CTR(FFacade)
		}

		~FFacade()
		{
			PCGEX_LOG_DTR(FFacade)
		}

		bool ShareSource(const FFacade* OtherManager) const { return this == OtherManager || OtherManager->Source == Source; }

		template <typename T>
		TSharedPtr<TBuffer<T>> FindBufferUnsafe(const FName FullName)
		{
			const TSharedPtr<FBufferBase>& Found = FindBufferUnsafe(BufferUID(FullName, PCGEx::GetMetadataType<T>()));
			if (!Found) { return nullptr; }
			return StaticCastSharedPtr<TBuffer<T>>(Found);
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> FindBuffer(const FName FullName)
		{
			const TSharedPtr<FBufferBase> Found = FindBuffer(BufferUID(FullName, PCGEx::GetMetadataType<T>()));
			if (!Found) { return nullptr; }
			return StaticCastSharedPtr<TBuffer<T>>(Found);
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetBuffer(FName FullName)
		{
			TSharedPtr<TBuffer<T>> NewBuffer = FindBuffer<T>(FullName);
			if (NewBuffer) { return NewBuffer; }

			{
				FWriteScopeLock WriteScopeLock(PoolLock);

				NewBuffer = FindBufferUnsafe<T>(FullName);
				if (NewBuffer) { return NewBuffer; }

				NewBuffer = MakeShared<TBuffer<T>>(Source, FullName, PCGEx::GetMetadataType<T>());
				NewBuffer->BufferIndex = Buffers.Num();

				Buffers.Add(StaticCastSharedPtr<FBufferBase>(NewBuffer));
				BufferMap.Add(NewBuffer->UID, NewBuffer);

				return NewBuffer;
			}
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetBroadcaster(const FPCGAttributePropertyInputSelector& InSelector, const bool bCaptureMinMax = false)
		{
			TSharedPtr<PCGEx::TAttributeGetter<T>> Getter = MakeShared<PCGEx::TAttributeGetter<T>>();
			if (!Getter->Prepare(InSelector, Source)) { return nullptr; }

			TSharedPtr<TBuffer<T>> Buffer = GetBuffer<T>(Getter->FullName);

			{
				FWriteScopeLock WriteScopeLock(Buffer->BufferLock);
				Buffer->PrepareForRead(false, Getter->GetAttribute());
				Getter->GrabAndDump(*Buffer->GetInValues(), bCaptureMinMax, Buffer->Min, Buffer->Max);
			}

			return Buffer;
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetBroadcaster(const FName& InName, const bool bCaptureMinMax = false)
		{
			FPCGAttributePropertyInputSelector Selector = FPCGAttributePropertyInputSelector();
			Selector.SetAttributeName(InName);
			return GetBroadcaster<T>(Selector, bCaptureMinMax);
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetScopedBroadcaster(const FPCGAttributePropertyInputSelector& InSelector)
		{
			if (!bSupportsScopedGet) { return GetBroadcaster<T>(InSelector); }

			TSharedPtr<PCGEx::TAttributeGetter<T>> Getter = MakeShared<PCGEx::TAttributeGetter<T>>();
			if (!Getter->Prepare(InSelector, Source)) { return nullptr; }

			TSharedPtr<TBuffer<T>> Buffer = GetBuffer<T>(Getter->FullName);
			Buffer->SetScopedGetter(Getter.ToSharedRef());

			return Buffer;
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetScopedBroadcaster(const FName& InName, const bool bCaptureMinMax = false)
		{
			FPCGAttributePropertyInputSelector Selector = FPCGAttributePropertyInputSelector();
			Selector.SetAttributeName(InName);
			return GetScopedBroadcaster<T>(Selector, bCaptureMinMax);
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetWritable(const FPCGMetadataAttribute<T>* InAttribute, bool bUninitialized)
		{
			TSharedPtr<TBuffer<T>> Buffer = GetBuffer<T>(InAttribute->Name);
			return Buffer->PrepareWrite(InAttribute->GetValue(PCGDefaultValueKey), InAttribute->AllowsInterpolation(), bUninitialized) ? Buffer : nullptr;
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetWritable(const FName InName, T DefaultValue, bool bAllowInterpolation, bool bUninitialized)
		{
			TSharedPtr<TBuffer<T>> Buffer = GetBuffer<T>(InName);
			return Buffer->PrepareWrite(DefaultValue, bAllowInterpolation, bUninitialized) ? Buffer : nullptr;
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetWritable(const FName InName, bool bUninitialized)
		{
			TSharedPtr<TBuffer<T>> Buffer = GetBuffer<T>(InName);
			return Buffer->PrepareWrite(bUninitialized) ? Buffer : nullptr;
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetReadable(const FName InName, const ESource InSource = ESource::In)
		{
			TSharedPtr<TBuffer<T>> Buffer = GetBuffer<T>(InName);
			if (!Buffer->PrepareRead(InSource, false))
			{
				Flush(Buffer);
				return nullptr;
			}

			return Buffer;
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetScopedReadable(const FName InName)
		{
			if (!bSupportsScopedGet) { return GetReadable<T>(InName); }

			TSharedPtr<TBuffer<T>> Buffer = GetBuffer<T>(InName);
			if (!Buffer->PrepareRead(ESource::In, true))
			{
				Flush(Buffer);
				return nullptr;
			}

			return Buffer;
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

		TSharedPtr<PCGExGeo::FPointBoxCloud> GetCloud(const EPCGExPointBoundsSource BoundsSource, const double Epsilon = DBL_EPSILON)
		{
			FWriteScopeLock WriteScopeLock(CloudLock);

			if (Cloud) { return Cloud; }

			Cloud = MakeShared<PCGExGeo::FPointBoxCloud>(GetIn(), BoundsSource, Epsilon);
			return Cloud;
		}

		const UPCGPointData* GetData(const ESource InSource) const { return Source->GetData(InSource); }
		const UPCGPointData* GetIn() const { return Source->GetIn(); }
		UPCGPointData* GetOut() const { return Source->GetOut(); }

		void Flush()
		{
			Buffers.Empty();
			BufferMap.Empty();
		}

		void Write(const TWeakPtr<PCGExMT::FTaskManager>& AsyncManagerPtr)
		{
			if (!AsyncManagerPtr.Pin()) { return; }

			for (int i = 0; i < Buffers.Num(); i++)
			{
				const TSharedPtr<FBufferBase>& Buffer = Buffers[i];
				if (Buffer->IsWritable()) { PCGExMT::Write(AsyncManagerPtr, Buffer); }
			}
		}

		void Fetch(const int32 StartIndex, const int32 Count) { for (const TSharedPtr<FBufferBase>& Buffer : Buffers) { Buffer->Fetch(StartIndex, Count); } }
		void Fetch(const uint64 Scope) { Fetch(PCGEx::H64A(Scope), PCGEx::H64B(Scope)); }

	protected:
		void Flush(const TSharedPtr<FBufferBase>& Buffer)
		{
			FWriteScopeLock WriteScopeLock(PoolLock);
			Buffers.RemoveAt(Buffer->BufferIndex);
			BufferMap.Remove(Buffer->UID);
			for (int i = 0; i < Buffers.Num(); i++) { Buffers[i].Get()->BufferIndex = i; }
		}
	};

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

		~FIdxCompound() = default;

		int32 Num() const { return CompoundedHashSet.Num(); }

		void ComputeWeights(
			const TArray<TSharedPtr<FFacade>>& Sources,
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
		TArray<TUniquePtr<FIdxCompound>> Compounds;

		FIdxCompoundList() { Compounds.Empty(); }
		~FIdxCompoundList() = default;

		int32 Num() const { return Compounds.Num(); }

		FORCEINLINE FIdxCompound* New(const int32 IOIndex, const int32 PointIndex)
		{
			FIdxCompound* NewPointCompound = Compounds.Add_GetRef(MakeUnique<FIdxCompound>()).Get();
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

		FORCEINLINE FIdxCompound* Get(const int32 Index) const { return Compounds[Index].Get(); }
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
