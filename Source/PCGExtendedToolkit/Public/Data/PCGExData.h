// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExHelpers.h"
#include "PCGExPointIO.h"
#include "PCGExAttributeHelpers.h"
#include "PCGExDataFilter.h"
#include "PCGExDetails.h"
#include "PCGExMT.h"
#include "Data/PCGPointData.h"
#include "Geometry/PCGExGeoPointBox.h"

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

	class FFacade;

	static uint64 BufferUID(const FName FullName, const EPCGMetadataTypes Type)
	{
		return PCGEx::H64(GetTypeHash(FullName), static_cast<int32>(Type));
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FBufferBase : public TSharedFromThis<FBufferBase>
	{
		friend class FFacade;

	protected:
		mutable FRWLock BufferLock;
		mutable FRWLock WriteLock;

		bool bScopedBuffer = false;

		TArrayView<const FPCGPoint> InPoints;
		TArrayView<FPCGPoint> OutPoints;

		EPCGMetadataTypes Type = EPCGMetadataTypes::Unknown;
		uint64 UID = 0;

	public:
		FName FullName = NAME_None;

		const FPCGMetadataAttributeBase* InAttribute = nullptr;
		FPCGMetadataAttributeBase* OutAttribute = nullptr;

		int32 BufferIndex = -1;
		const TSharedRef<FPointIO> Source;


		FBufferBase(const TSharedRef<FPointIO>& InSource, const FName InFullName):
			FullName(InFullName), Source(InSource)
		{
			PCGEX_LOG_CTR(FBufferBase)
		}

		uint64 GetUID() const { return UID; }

		void SetType(const EPCGMetadataTypes InType)
		{
			Type = InType;
			UID = BufferUID(FullName, InType);
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
	class /*PCGEXTENDEDTOOLKIT_API*/ TBuffer final : public FBufferBase
	{
		friend class FFacade;

	protected:
		TUniquePtr<FPCGAttributeAccessor<T>> InAccessor;
		const FPCGMetadataAttribute<T>* TypedInAttribute = nullptr;

		TUniquePtr<FPCGAttributeAccessor<T>> OutAccessor;
		FPCGMetadataAttribute<T>* TypedOutAttribute = nullptr;

		TSharedPtr<TArray<T>> InValues;
		TSharedPtr<TArray<T>> OutValues;

	public:
		T Min = T{};
		T Max = T{};

		TSharedPtr<PCGEx::TAttributeBroadcaster<T>> ScopedBroadcaster;

		virtual bool IsScoped() override { return bScopedBuffer || ScopedBroadcaster; }

		TBuffer(const TSharedRef<FPointIO>& InSource, const FName InFullName):
			FBufferBase(InSource, InFullName)
		{
			SetType(PCGEx::GetMetadataType<T>());
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

	protected:
		void PrepareReadInternal(const bool bScoped, const FPCGMetadataAttributeBase* Attribute)
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

		void PrepareWriteInternal(FPCGMetadataAttributeBase* Attribute, const bool bUninitialized, const T& InDefaultValue)
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

	public:
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

			check(InMetadata)

			// 'template' spec required for clang on mac, not sure why.
			// ReSharper disable once CppRedundantTemplateKeyword
			TypedInAttribute = InMetadata->template GetConstTypedAttribute<T>(FullName);
			if (!TypedInAttribute) { return false; }

			InAccessor = MakeUnique<FPCGAttributeAccessor<T>>(TypedInAttribute, InMetadata);

			if (!TypedInAttribute || !InAccessor.IsValid())
			{
				TypedInAttribute = nullptr;
				InAccessor = nullptr;
				return false;
			}

			PrepareReadInternal(bScoped, TypedInAttribute);

			if (!bScopedBuffer)
			{
				TArrayView<T> InRange = MakeArrayView(InValues->GetData(), InValues->Num());
				InAccessor->GetRange(InRange, 0, *Source->GetInKeys());
			}

			return true;
		}

		bool PrepareWrite(const T& DefaultValue, bool bAllowInterpolation, const bool bUninitialized = false)
		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			if (OutValues) { return true; }

			TypedOutAttribute = Source->FindOrCreateAttribute(FullName, DefaultValue, bAllowInterpolation);
			OutAccessor = MakeUnique<FPCGAttributeAccessor<T>>(TypedOutAttribute, Source->GetOut()->Metadata);

			if (!TypedOutAttribute || !OutAccessor.IsValid())
			{
				TypedOutAttribute = nullptr;
				return false;
			}

			PrepareWriteInternal(TypedOutAttribute, bUninitialized, DefaultValue);

			if (!bUninitialized)
			{
				// 'template' spec required for clang on mac, not sure why.
				// ReSharper disable once CppRedundantTemplateKeyword
				if (Source->GetIn() && Source->GetIn()->Metadata->template GetConstTypedAttribute<T>(FullName))
				{
					// TODO : Scoped get would be better here
					// Get existing values
					TArrayView<T> OutRange = MakeArrayView(OutValues->GetData(), OutValues->Num());
					OutAccessor->GetRange(OutRange, 0, *Source->GetOutKeys(false));
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
				// 'template' spec required for clang on mac, not sure why.
				// ReSharper disable once CppRedundantTemplateKeyword
				if (const FPCGMetadataAttribute<T>* ExistingAttribute = Source->GetIn()->Metadata->template GetConstTypedAttribute<T>(FullName))
				{
					return PrepareWrite(
						ExistingAttribute->GetValue(PCGDefaultValueKey),
						ExistingAttribute->AllowsInterpolation(),
						bUninitialized);
				}
			}

			return PrepareWrite(T{}, true, bUninitialized);
		}

		void SetScopedGetter(const TSharedRef<PCGEx::TAttributeBroadcaster<T>>& Getter)
		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			PrepareReadInternal(true, Getter->GetAttribute());
			ScopedBroadcaster = Getter;

			PCGEx::InitArray(InValues, Source->GetNum());
		}

		virtual void Write() override
		{
			if (!IsWritable() || !OutAccessor || !OutValues || !TypedOutAttribute) { return; }

			TArrayView<const T> View = MakeArrayView(OutValues->GetData(), OutValues->Num());
			OutAccessor->SetRange(View, 0, *Source->GetOutKeys(true).Get());
		}

		virtual void Fetch(const int32 StartIndex, const int32 Count) override
		{
			if (!IsScoped()) { return; }
			if (ScopedBroadcaster) { ScopedBroadcaster->Fetch(*InValues, StartIndex, Count); }
			if (InAccessor.IsValid())
			{
				TArrayView<T> ReadRange = MakeArrayView(InValues->GetData() + StartIndex, Count);
				InAccessor->GetRange(ReadRange, StartIndex, *Source->GetInKeys());
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

	class /*PCGEXTENDEDTOOLKIT_API*/ FFacade : public TSharedFromThis<FFacade>
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

		FORCEINLINE bool IsDataValid(const ESource InSource) const { return Source->IsDataValid(InSource); }

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

				NewBuffer = MakeShared<TBuffer<T>>(Source, FullName);
				NewBuffer->BufferIndex = Buffers.Num();

				Buffers.Add(StaticCastSharedPtr<FBufferBase>(NewBuffer));
				BufferMap.Add(NewBuffer->UID, NewBuffer);

				return NewBuffer;
			}
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetBroadcaster(const FPCGAttributePropertyInputSelector& InSelector, const bool bCaptureMinMax = false)
		{
			TSharedPtr<PCGEx::TAttributeBroadcaster<T>> Getter = MakeShared<PCGEx::TAttributeBroadcaster<T>>();
			if (!Getter->Prepare(InSelector, Source)) { return nullptr; }

			TSharedPtr<TBuffer<T>> Buffer = GetBuffer<T>(Getter->FullName);

			{
				FWriteScopeLock WriteScopeLock(Buffer->BufferLock);
				Buffer->PrepareReadInternal(false, Getter->GetAttribute());
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

			TSharedPtr<PCGEx::TAttributeBroadcaster<T>> Getter = MakeShared<PCGEx::TAttributeBroadcaster<T>>();
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

			// 'template' spec required for clang on mac, not sure why.
			// ReSharper disable once CppRedundantTemplateKeyword
			return Data->Metadata->template GetConstTypedAttribute<T>(InName);
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

		void Write(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager);
		void WriteBuffersAsCallbacks(const TSharedPtr<PCGExMT::FTaskGroup>& TaskGroup);

		void Fetch(const int32 StartIndex, const int32 Count) { for (const TSharedPtr<FBufferBase>& Buffer : Buffers) { Buffer->Fetch(StartIndex, Count); } }
		void Fetch(const uint64 Scope) { Fetch(PCGEx::H64A(Scope), PCGEx::H64B(Scope)); }

	protected:
		void Flush(const TSharedPtr<FBufferBase>& Buffer)
		{
			FWriteScopeLock WriteScopeLock(PoolLock);
			Buffers.RemoveAt(Buffer->BufferIndex);
			BufferMap.Remove(Buffer->GetUID());
			for (int i = 0; i < Buffers.Num(); i++) { Buffers[i].Get()->BufferIndex = i; }
		}
	};

#pragma endregion

#pragma region Compound

	struct /*PCGEXTENDEDTOOLKIT_API*/ FUnionData
	{
	protected:
		mutable FRWLock UnionLock;

	public:
		//int32 Index = 0;
		TSet<int32> IOIndices;
		TSet<uint64> ItemHashSet;

		FUnionData()
		{
		}

		~FUnionData() = default;

		int32 Num() const { return ItemHashSet.Num(); }

		void ComputeWeights(
			const TArray<TSharedPtr<FFacade>>& Sources,
			const TMap<uint32, int32>& SourcesIdx,
			const FPCGPoint& Target,
			const FPCGExDistanceDetails& InDistanceDetails,
			TArray<int32>& OutIOIdx,
			TArray<int32>& OutPointsIdx,
			TArray<double>& OutWeights) const;

		uint64 Add(const int32 IOIndex, const int32 PointIndex);

		void Reset()
		{
			IOIndices.Reset();
			ItemHashSet.Reset();
		}
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FUnionMetadata
	{
		TArray<FUnionData*> Entries;
		bool bIsAbstract = false;

		FUnionMetadata() { Entries.Empty(); }

		~FUnionMetadata()
		{
			for (const FUnionData* Entry : Entries) { delete Entry; }
		}

		int32 Num() const { return Entries.Num(); }

		FUnionData* NewEntry(const int32 IOIndex, const int32 ItemIndex);

		uint64 Append(const int32 Index, const int32 IOIndex, const int32 ItemIndex);
		bool IOIndexOverlap(const int32 InIdx, const TSet<int32>& InIndices);

		FORCEINLINE FUnionData* Get(const int32 Index) const { return Entries[Index]; }
	};

#pragma endregion

#pragma region Data Marking

	template <typename T>
	static FPCGMetadataAttribute<T>* WriteMark(const TSharedRef<FPointIO>& PointIO, const FName MarkID, T MarkValue)
	{
		PointIO->DeleteAttribute(MarkID);
		FPCGMetadataAttribute<T>* Mark = PointIO->CreateAttribute<T>(MarkID, MarkValue, false, true);
		Mark->SetDefaultValue(MarkValue);
		return Mark;
	}


	template <typename T>
	static bool TryReadMark(UPCGMetadata* Metadata, const FName MarkID, T& OutMark)
	{
		// 'template' spec required for clang on mac, not sure why.
		// ReSharper disable once CppRedundantTemplateKeyword
		const FPCGMetadataAttribute<T>* Mark = Metadata->template GetConstTypedAttribute<T>(MarkID);
		if (!Mark) { return false; }
		OutMark = Mark->GetValue(PCGInvalidEntryKey);
		return true;
	}

	template <typename T>
	static bool TryReadMark(const TSharedRef<FPointIO>& PointIO, const FName MarkID, T& OutMark)
	{
		return TryReadMark(PointIO->GetIn() ? PointIO->GetIn()->Metadata : PointIO->GetOut()->Metadata, MarkID, OutMark);
	}

	static void WriteId(const TSharedRef<FPointIO>& PointIO, const FName IdName, const int64 Id)
	{
		FString OutId;
		PointIO->Tags->Add(IdName.ToString(), Id, OutId);
		if (PointIO->GetOut()) { WriteMark(PointIO, IdName, Id); }
	}

	static UPCGPointData* GetMutablePointData(FPCGContext* Context, const FPCGTaggedData& Source)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Source.Data);
		if (!SpatialData) { return nullptr; }

		const UPCGPointData* PointData = SpatialData->ToPointData(Context);
		if (!PointData) { return nullptr; }

		return const_cast<UPCGPointData*>(PointData);
	}

	static void CopyValues(
		const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager,
		const PCGEx::FAttributeIdentity& Identity,
		const TSharedPtr<FPointIO>& Source,
		const TSharedPtr<FPointIO>& Target,
		const TArrayView<const int32>& SourceIndices,
		const int32 TargetIndex = 0)
	{
		PCGMetadataAttribute::CallbackWithRightType(
			static_cast<uint16>(Identity.UnderlyingType),
			[&](auto DummyValue) -> void
			{
				using T = decltype(DummyValue);
				TArray<T> RawValues;

				// 'template' spec required for clang on mac, not sure why.
				// ReSharper disable once CppRedundantTemplateKeyword
				const FPCGMetadataAttribute<T>* SourceAttribute = Source->GetIn()->Metadata->template GetConstTypedAttribute<T>(Identity.Name);

				const TSharedPtr<TBuffer<T>> TargetBuffer = MakeShared<TBuffer<T>>(Target.ToSharedRef(), Identity.Name);
				TargetBuffer->PrepareWrite(SourceAttribute->GetValue(PCGDefaultValueKey), SourceAttribute->AllowsInterpolation(), true);

				TUniquePtr<FPCGAttributeAccessor<T>> InAccessor = MakeUnique<FPCGAttributeAccessor<T>>(SourceAttribute, Source->GetIn()->Metadata);
				TArrayView<T> InRange = MakeArrayView(TargetBuffer->GetOutValues()->GetData() + TargetIndex, SourceIndices.Num());
				InAccessor->GetRange(InRange, 0, *Source->GetInKeys());

				PCGExMT::Write(AsyncManager, TargetBuffer);
			});
	}

#pragma endregion
}
