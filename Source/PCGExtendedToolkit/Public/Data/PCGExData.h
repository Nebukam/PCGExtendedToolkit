// Copyright 2024 Timothé Lapetite and contributors
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

#pragma region DATA MACROS

#ifndef PCGEX_DATA_MACROS
#define PCGEX_DATA_MACROS

#define PCGEX_INIT_IO_VOID(_IO, _INIT) if (!_IO->InitializeOutput(_INIT)) { return; }
#define PCGEX_INIT_IO(_IO, _INIT) if (!_IO->InitializeOutput(_INIT)) { return false; }

#define PCGEX_CLEAR_IO_VOID(_IO) if (!_IO->InitializeOutput(PCGExData::EIOInit::None)) { return; }
#define PCGEX_CLEAR_IO(_IO) if (!_IO->InitializeOutput(PCGExData::EIOInit::None)) { return false; }

#endif
#pragma endregion

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
	enum class EBufferPreloadType : uint8
	{
		RawAttribute = 0,
		BroadcastFromName,
		BroadcastFromSelector,
	};

	enum class EBufferInit : uint8
	{
		Inherit = 0,
		New,
	};

	enum class EBufferLevel : uint8
	{
		Local = 0, // Per point data
		Global     // Per dataset data (Tags etc)
	};

	PCGEX_CTX_STATE(State_MergingData);

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

		bool bReadComplete = false;

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

		EPCGMetadataTypes GetType() const { return Type; }

		template <typename T>
		bool IsA() const { return Type == PCGEx::GetMetadataType<T>(); }

		virtual ~FBufferBase()
		{
			PCGEX_LOG_DTR(FBufferBase)
		}

		virtual void Write()
		{
		}

		virtual void Fetch(const PCGExMT::FScope& Scope)
		{
		}

		virtual bool IsScoped() { return bScopedBuffer; }
		virtual bool IsWritable() { return false; }
		virtual bool IsReadable() { return false; }

		FORCEINLINE bool GetAllowsInterpolation() const { return OutAttribute ? OutAttribute->AllowsInterpolation() : InAttribute ? InAttribute->AllowsInterpolation() : false; }
	};

	template <typename T, EBufferLevel BufferLevel = EBufferLevel::Local>
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

		TSharedPtr<PCGEx::TAttributeBroadcaster<T>> InternalBroadcaster;

		virtual bool IsScoped() override { return bScopedBuffer || InternalBroadcaster; }

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
		FORCEINLINE const T& ReadImmediate(const int32 Index) const { return TypedInAttribute->GetValueFromItemKey(InPoints[Index].MetadataEntry); }

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

		void PrepareWriteInternal(FPCGMetadataAttributeBase* Attribute, const T& InDefaultValue, const EBufferInit Init)
		{
			if (OutValues) { return; }

			TArray<FPCGPoint>& OutPts = Source->GetMutablePoints();
			const int32 NumPoints = OutPts.Num();
			OutPoints = MakeArrayView(OutPts.GetData(), NumPoints);

			OutValues = MakeShared<TArray<T>>();
			OutValues->Init(InDefaultValue, NumPoints);

			if (Attribute)
			{
				// Assume that if we write data, it's not to delete it.
				Source->GetContext()->AddProtectedAttributeName(Attribute->Name);
			}

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
					Fetch(PCGExMT::FScope(0, InValues->Num()));
					bReadComplete = true;
					bScopedBuffer = false;
				}

				if (InSource == ESource::In && OutValues && InValues == OutValues)
				{
					check(false)
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

			if (!InAccessor.IsValid())
			{
				TypedInAttribute = nullptr;
				InAccessor = nullptr;
				return false;
			}

			PrepareReadInternal(bScoped, TypedInAttribute);

			if (!bScopedBuffer && !bReadComplete)
			{
				TArrayView<T> InRange = MakeArrayView(InValues->GetData(), InValues->Num());
				InAccessor->GetRange(InRange, 0, *Source->GetInKeys());
				bReadComplete = true;
			}

			return true;
		}


		bool PrepareBroadcast(const FPCGAttributePropertyInputSelector& InSelector, const bool bCaptureMinMax = false, const bool bScoped = false)
		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			if (InValues)
			{
				if (bScopedBuffer && !bScoped)
				{
					// Un-scoping reader.
					InternalBroadcaster->GrabAndDump(*InValues, bCaptureMinMax, Min, Max);
					bReadComplete = true;
					bScopedBuffer = false;
					InternalBroadcaster.Reset();
				}

				if (OutValues && InValues == OutValues)
				{
					check(false)
					// Out-source broadcaster was created before writer, this is bad?
					InValues = nullptr;
				}
				else
				{
					return true;
				}
			}

			InternalBroadcaster = MakeShared<PCGEx::TAttributeBroadcaster<T>>();
			if (!InternalBroadcaster->Prepare(InSelector, Source))
			{
				TypedInAttribute = nullptr;
				InAccessor = nullptr;
				return false;
			}

			PrepareReadInternal(bScoped, InternalBroadcaster->GetAttribute());

			if (!bScopedBuffer && !bReadComplete)
			{
				InternalBroadcaster->GrabAndDump(*InValues, bCaptureMinMax, Min, Max);
				bReadComplete = true;
				InternalBroadcaster.Reset();
			}

			return true;
		}

		bool PrepareWrite(const T& DefaultValue, bool bAllowInterpolation, EBufferInit Init = EBufferInit::Inherit)
		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			if (OutValues)
			{
				check(OutValues->Num() == Source->GetOut()->GetPoints().Num())
				return true;
			}

			TypedOutAttribute = Source->FindOrCreateAttribute(FullName, DefaultValue, bAllowInterpolation);

			OutAccessor = MakeUnique<FPCGAttributeAccessor<T>>(TypedOutAttribute, Source->GetOut()->Metadata);

			if (!TypedOutAttribute || !OutAccessor.IsValid())
			{
				TypedOutAttribute = nullptr;
				return false;
			}

			PrepareWriteInternal(TypedOutAttribute, DefaultValue, Init);

			const int32 ExistingEntryCount = TypedOutAttribute->GetNumberOfEntries();
			const bool bHasIn = Source->GetIn() ? true : false;

			auto GrabExistingValues = [&]()
			{
				TUniquePtr<FPCGAttributeAccessorKeysPoints> TempOutKeys = MakeUnique<FPCGAttributeAccessorKeysPoints>(MakeArrayView(Source->GetMutablePoints().GetData(), OutValues->Num()));
				TArrayView<T> OutRange = MakeArrayView(OutValues->GetData(), OutValues->Num());
				OutAccessor->GetRange(OutRange, 0, *TempOutKeys.Get());
			};

			if (Init == EBufferInit::Inherit)
			{
				GrabExistingValues();
				/*
				if (!bHasIn && ExistingEntryCount != 0) { GrabExistingValues(); }
				else if (bHasIn)
				{
					if (InValues && bReadComplete)
					{
						// Leverage prefetched data					
						int32 NumToCopy = FMath::Min(InValues->Num(), OutValues->Num());
						if constexpr (std::is_trivial_v<T>)
						{
							FMemory::Memcpy(OutValues->GetData(), InValues->GetData(), NumToCopy * sizeof(T));
						}
						else
						{
							for (int32 i = 0; i < NumToCopy; i++) { *(OutValues->GetData() + i) = *(InValues->GetData() + i); }
						}
					}
					else
					{
						UPCGMetadata* InMetadata = Source->GetIn()->Metadata;

						// 'template' spec required for clang on mac, not sure why.
						// ReSharper disable once CppRedundantTemplateKeyword
						if (const FPCGMetadataAttribute<T>* TempInAttribute = InMetadata->template GetConstTypedAttribute<T>(FullName))
						{
							TUniquePtr<FPCGAttributeAccessor<T>> TempInAccessor = MakeUnique<FPCGAttributeAccessor<T>>(TempInAttribute, InMetadata);
							TArrayView<T> OutRange = MakeArrayView(OutValues->GetData(), FMath::Min(Source->GetNum(), OutValues->Num()));
							TempInAccessor->GetRange(OutRange, 0, *Source->GetInKeys());
						}
					}
				}
				*/
			}
			else if (!bHasIn && ExistingEntryCount != 0) { GrabExistingValues(); }

			return true;
		}

		bool PrepareWrite(const EBufferInit Init = EBufferInit::Inherit)
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
						Init);
				}
			}

			return PrepareWrite(T{}, true, Init);
		}

		virtual void Write() override
		{
			if (!IsWritable() || !OutAccessor || !OutValues || !TypedOutAttribute) { return; }

			if (!Source->GetOut())
			{
				UE_LOG(LogTemp, Error, TEXT("Attempting to write data to an output that's not initialized!"));
				return;
			}

			TRACE_CPUPROFILER_EVENT_SCOPE(TBuffer::Write);

			TArrayView<const T> View = MakeArrayView(OutValues->GetData(), OutValues->Num());
			OutAccessor->SetRange(View, 0, *Source->GetOutKeys(true).Get());
		}

		virtual void Fetch(const PCGExMT::FScope& Scope) override
		{
			if (!IsScoped() || bReadComplete) { return; }
			if (InternalBroadcaster) { InternalBroadcaster->Fetch(*InValues, Scope); }
			if (InAccessor.IsValid())
			{
				TArrayView<T> ReadRange = MakeArrayView(InValues->GetData() + Scope.Start, Scope.Count);
				InAccessor->GetRange(ReadRange, Scope.Start, *Source->GetInKeys());
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
			InternalBroadcaster.Reset();
		}
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FFacade : public TSharedFromThis<FFacade>
	{
		mutable FRWLock BufferLock;
		mutable FRWLock CloudLock;

	public:
		TSharedRef<FPointIO> Source;
		TArray<TSharedPtr<FBufferBase>> Buffers;
		TMap<uint64, TSharedPtr<FBufferBase>> BufferMap;
		TSharedPtr<PCGExGeo::FPointBoxCloud> Cloud;

		bool bSupportsScopedGet = false;

		FORCEINLINE int32 GetNum(const ESource InSource = ESource::In) const { return Source->GetNum(InSource); }
		FORCEINLINE TArray<FPCGPoint>& GetMutablePoints() const { return Source->GetMutablePoints(); }

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
				FWriteScopeLock WriteScopeLock(BufferLock);

				NewBuffer = FindBufferUnsafe<T>(FullName);
				if (NewBuffer) { return NewBuffer; }

				NewBuffer = MakeShared<TBuffer<T>>(Source, FullName);
				NewBuffer->BufferIndex = Buffers.Num();

				Buffers.Add(StaticCastSharedPtr<FBufferBase>(NewBuffer));
				BufferMap.Add(NewBuffer->UID, NewBuffer);

				return NewBuffer;
			}
		}

#pragma region Writable

		template <typename T>
		TSharedPtr<TBuffer<T>> GetWritable(const FName InName, T DefaultValue, bool bAllowInterpolation, EBufferInit Init)
		{
			TSharedPtr<TBuffer<T>> Buffer = GetBuffer<T>(InName);
			return Buffer->PrepareWrite(DefaultValue, bAllowInterpolation, Init) ? Buffer : nullptr;
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetWritable(const FPCGMetadataAttribute<T>* InAttribute, EBufferInit Init)
		{
			return GetWritable(InAttribute->Name, InAttribute->GetValue(PCGDefaultValueKey), InAttribute->AllowsInterpolation(), Init);
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetWritable(const FName InName, EBufferInit Init)
		{
			TSharedPtr<TBuffer<T>> Buffer = GetBuffer<T>(InName);
			return Buffer->PrepareWrite(Init) ? Buffer : nullptr;
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

#pragma endregion

#pragma region Readable

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

#pragma endregion

#pragma region Broadcasters

		template <typename T>
		TSharedPtr<TBuffer<T>> GetBroadcaster(const FPCGAttributePropertyInputSelector& InSelector, const bool bCaptureMinMax = false)
		{
			TSharedPtr<TBuffer<T>> Buffer = GetBuffer<T>(PCGEx::GetSelectorFullName<false>(InSelector, Source->GetIn()));
			if (!Buffer->PrepareBroadcast(InSelector, bCaptureMinMax, false))
			{
				Flush(Buffer);
				return nullptr;
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

			TSharedPtr<TBuffer<T>> Buffer = GetBuffer<T>(PCGEx::GetSelectorFullName<true>(InSelector, Source->GetIn()));
			if (!Buffer->PrepareBroadcast(InSelector, false, false))
			{
				Flush(Buffer);
				return nullptr;
			}

			return Buffer;
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetScopedBroadcaster(const FName& InName)
		{
			FPCGAttributePropertyInputSelector Selector = FPCGAttributePropertyInputSelector();
			Selector.SetAttributeName(InName);
			return GetScopedBroadcaster<T>(Selector);
		}

#pragma endregion

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

		TSharedPtr<PCGExGeo::FPointBoxCloud> GetCloud(const EPCGExPointBoundsSource BoundsSource, const double Expansion = DBL_EPSILON)
		{
			FWriteScopeLock WriteScopeLock(CloudLock);

			if (Cloud) { return Cloud; }

			Cloud = MakeShared<PCGExGeo::FPointBoxCloud>(GetIn(), BoundsSource, Expansion);
			return Cloud;
		}

		const UPCGPointData* GetData(const ESource InSource) const { return Source->GetData(InSource); }
		const UPCGPointData* GetIn() const { return Source->GetIn(); }
		UPCGPointData* GetOut() const { return Source->GetOut(); }

		void MarkCurrentBuffersReadAsComplete();

		void Flush()
		{
			FWriteScopeLock WriteScopeLock(BufferLock);
			Buffers.Empty();
			BufferMap.Empty();
		}

		void Write(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager);
		FPlatformTypes::int32 WriteBuffersAsCallbacks(const TSharedPtr<PCGExMT::FTaskGroup>& TaskGroup);
		void WriteBuffers(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, PCGExMT::FCompletionCallback&& Callback);

		void Fetch(const PCGExMT::FScope& Scope) { for (const TSharedPtr<FBufferBase>& Buffer : Buffers) { Buffer->Fetch(Scope); } }

	protected:
		void Flush(const TSharedPtr<FBufferBase>& Buffer)
		{
			FWriteScopeLock WriteScopeLock(BufferLock);
			Buffers.RemoveAt(Buffer->BufferIndex);
			BufferMap.Remove(Buffer->GetUID());
			for (int i = 0; i < Buffers.Num(); i++) { Buffers[i].Get()->BufferIndex = i; }
		}
	};

#pragma endregion

#pragma region Facade prep

	struct /*PCGEXTENDEDTOOLKIT_API*/ FReadableBufferConfig
	{
		EBufferPreloadType Mode = EBufferPreloadType::RawAttribute;
		FPCGAttributePropertyInputSelector Selector;
		PCGEx::FAttributeIdentity Identity;

		FReadableBufferConfig(const FReadableBufferConfig& Other)
			: Mode(Other.Mode), Selector(Other.Selector), Identity(Other.Identity)
		{
		}

		FReadableBufferConfig(const PCGEx::FAttributeIdentity& InIdentity, EBufferPreloadType InMode = EBufferPreloadType::RawAttribute)
			: Mode(InMode), Identity(InIdentity)
		{
		}

		FReadableBufferConfig(const FName InName, const EPCGMetadataTypes InUnderlyingType, EBufferPreloadType InMode = EBufferPreloadType::RawAttribute)
			: Mode(InMode), Identity(InName, InUnderlyingType, false)
		{
		}

		FReadableBufferConfig(const FPCGAttributePropertyInputSelector& InSelector, const EPCGMetadataTypes InUnderlyingType)
			: Mode(EBufferPreloadType::BroadcastFromSelector), Selector(InSelector), Identity(InSelector.GetName(), InUnderlyingType, false)
		{
		}

		bool Validate(FPCGExContext* InContext, const TSharedRef<FFacade>& InFacade) const;
		void Fetch(const TSharedRef<FFacade>& InFacade, const PCGExMT::FScope& Scope) const;
		void Read(const TSharedRef<FFacade>& InFacade) const;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FFacadePreloader : public TSharedFromThis<FFacadePreloader>
	{
	protected:
		TWeakPtr<FFacade> InternalDataFacadePtr;

	public:
		TArray<FReadableBufferConfig> BufferConfigs;

		FFacadePreloader()
		{
		}

		bool IsEmpty() const { return BufferConfigs.IsEmpty(); }
		int32 Num() const { return BufferConfigs.Num(); }

		bool Validate(FPCGExContext* InContext, const TSharedRef<FFacade>& InFacade) const;

		void Register(FPCGExContext* InContext, const PCGEx::FAttributeIdentity& InIdentity)
		{
			for (const FReadableBufferConfig& ExistingConfig : BufferConfigs)
			{
				if (ExistingConfig.Identity == InIdentity) { return; }
			}

			BufferConfigs.Emplace(InIdentity.Name, InIdentity.UnderlyingType);
		}

		template <typename T>
		void Register(FPCGExContext* InContext, const FPCGAttributePropertyInputSelector& InSelector, bool bCaptureMinMax = false)
		{
			EPCGMetadataTypes Type = PCGEx::GetMetadataType<T>();
			for (const FReadableBufferConfig& ExistingConfig : BufferConfigs)
			{
				if (ExistingConfig.Selector == InSelector &&
					ExistingConfig.Identity.UnderlyingType == Type)
				{
					return;
				}
			}

			BufferConfigs.Emplace(InSelector, Type);
		}

		template <typename T>
		void Register(FPCGExContext* InContext, const FName InName, EBufferPreloadType InMode = EBufferPreloadType::RawAttribute)
		{
			EPCGMetadataTypes Type = PCGEx::GetMetadataType<T>();
			for (const FReadableBufferConfig& ExistingConfig : BufferConfigs)
			{
				if (ExistingConfig.Identity.Name == InName &&
					ExistingConfig.Identity.UnderlyingType == Type)
				{
					return;
				}
			}

			BufferConfigs.Emplace(InName, Type, InMode);
		}

		void Fetch(const TSharedRef<FFacade>& InFacade, const PCGExMT::FScope& Scope) const;
		void Read(const TSharedRef<FFacade>& InFacade, const int32 ConfigIndex) const;

		///

		using CompletionCallback = std::function<void()>;
		CompletionCallback OnCompleteCallback;

		void StartLoading(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const TSharedRef<FFacade>& InDataFacade, const TSharedPtr<PCGExMT::FAsyncMultiHandle>& InParentHandle = nullptr);

	protected:
		void OnLoadingEnd() const;
	};

#pragma endregion

#pragma region Compound

	class /*PCGEXTENDEDTOOLKIT_API*/ FUnionData : public TSharedFromThis<FUnionData>
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
			const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails,
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

	class /*PCGEXTENDEDTOOLKIT_API*/ FUnionMetadata : public TSharedFromThis<FUnionMetadata>
	{
	public:
		TArray<TSharedPtr<FUnionData>> Entries;
		bool bIsAbstract = false;

		FUnionMetadata() { Entries.Empty(); }

		~FUnionMetadata()
		{
		}

		int32 Num() const { return Entries.Num(); }

		TSharedPtr<FUnionData> NewEntry(const int32 IOIndex, const int32 ItemIndex);

		uint64 Append(const int32 Index, const int32 IOIndex, const int32 ItemIndex);
		bool IOIndexOverlap(const int32 InIdx, const TSet<int32>& InIndices);

		FORCEINLINE TSharedPtr<FUnionData> Get(const int32 Index) const { return Entries.IsValidIndex(Index) ? Entries[Index] : nullptr; }
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
		PointIO->Tags->Set<int64>(IdName.ToString(), Id);
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

#pragma endregion

	static TSharedPtr<FFacade> TryGetSingleFacade(FPCGExContext* InContext, const FName InputPinLabel, const bool bThrowError)
	{
		TSharedPtr<FFacade> SingleFacade;
		if (const TSharedPtr<FPointIO> SingleIO = TryGetSingleInput(InContext, InputPinLabel, bThrowError))
		{
			SingleFacade = MakeShared<FFacade>(SingleIO.ToSharedRef());
		}

		return SingleFacade;
	}

	template <bool bReverse = false>
	static void GetTransforms(const TArray<FPCGPoint>& InPoints, TArray<FTransform>& OutTransforms)
	{
		PCGEx::InitArray(OutTransforms, InPoints.Num());
		const int32 MaxIndex = InPoints.Num() - 1;
		if constexpr (bReverse) { for (int i = 0; i <= MaxIndex; i++) { OutTransforms[i] = InPoints[i].Transform; } }
		else { for (int i = 0; i <= MaxIndex; i++) { OutTransforms[MaxIndex - i] = InPoints[i].Transform; } }
	}
}
