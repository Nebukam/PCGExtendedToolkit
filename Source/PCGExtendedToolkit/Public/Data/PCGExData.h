// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExtendedToolkit.h"
#include "PCGExHelpers.h"
#include "PCGExMath.h"
#include "PCGExPointIO.h"
#include "PCGExAttributeHelpers.h"
#include "PCGExDataFilter.h"
#include "PCGExDetails.h"
#include "PCGExMT.h"
#include "Data/PCGPointData.h"

#include "PCGExData.generated.h"

#pragma region DATA MACROS

#ifndef PCGEX_DATA_MACROS
#define PCGEX_DATA_MACROS

#define PCGEX_INIT_IO_VOID(_IO, _INIT) if (!_IO->InitializeOutput(_INIT)) { return; }
#define PCGEX_INIT_IO(_IO, _INIT) if (!_IO->InitializeOutput(_INIT)) { return false; }

#define PCGEX_CLEAR_IO_VOID(_IO) if (!_IO->InitializeOutput(PCGExData::EIOInit::NoInit)) { return; }
#define PCGEX_CLEAR_IO(_IO) if (!_IO->InitializeOutput(PCGExData::EIOInit::NoInit)) { return false; }

#endif
#pragma endregion

namespace PCGExGeo
{
	class FPointBoxCloud;
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeGatherDetails : public FPCGExNameFiltersDetails
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

	class PCGEXTENDEDTOOLKIT_API FBufferBase : public TSharedFromThis<FBufferBase>
	{
		friend class FFacade;

	protected:
		mutable FRWLock BufferLock;
		mutable FRWLock WriteLock;

		bool bScopedBuffer = false;

		EPCGMetadataTypes Type = EPCGMetadataTypes::Unknown;
		uint64 UID = 0;

		bool bIsNewOutput = false;
		std::atomic<bool> bIsEnabled{true}; // BUG : Need to have a better look at why we hang when this is false
		FName TargetOutputName = NAME_None;

	public:
		FName FullName = NAME_None;

		bool IsEnabled() const { return bIsEnabled.load(std::memory_order_acquire); }
		void Disable() { bIsEnabled.store(false, std::memory_order_release); }
		void Enable() { bIsEnabled.store(true, std::memory_order_release); }

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

		EPCGMetadataTypes GetType() const { return Type; }

		template <typename T>
		bool IsA() const { return Type == PCGEx::GetMetadataType<T>(); }

		virtual ~FBufferBase() = default;

		virtual bool EnsureReadable() { return false; }

		virtual void Write(const bool bEnsureValidKeys = true)
		{
		}

		virtual void Fetch(const PCGExMT::FScope& Scope)
		{
		}

		virtual bool IsScoped() { return bScopedBuffer; }
		virtual bool IsWritable() PCGEX_NOT_IMPLEMENTED_RET(FBuffer::IsWritable, false)
		virtual bool IsReadable() PCGEX_NOT_IMPLEMENTED_RET(FBuffer::IsReadable, false)
		virtual bool IsReadingFromWrite() PCGEX_NOT_IMPLEMENTED_RET(FBuffer::IsReadingFromWrite, false)

		virtual void SetTargetOutputName(const FName InName);

		virtual PCGEx::FAttributeIdentity GetTargetOutputIdentity()
		PCGEX_NOT_IMPLEMENTED_RET(FBuffer::GetTargetOutputIdentity, PCGEx::FAttributeIdentity())

		virtual bool OutputsToDifferentName() const;

		bool GetAllowsInterpolation() const { return OutAttribute ? OutAttribute->AllowsInterpolation() : InAttribute ? InAttribute->AllowsInterpolation() : false; }

	protected:
		void SetType(const EPCGMetadataTypes InType)
		{
			Type = InType;
			UID = BufferUID(FullName, InType);
		}
	};

	template <typename T, EBufferLevel BufferLevel = EBufferLevel::Local>
	class TBuffer final : public FBufferBase
	{
		friend class FFacade;

	protected:
		const FPCGMetadataAttribute<T>* TypedInAttribute = nullptr;
		FPCGMetadataAttribute<T>* TypedOutAttribute = nullptr;

		TSharedPtr<TArray<T>> InValues;
		TSharedPtr<TArray<T>> OutValues;

		T InternalDefaultValue = T{};

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

		virtual bool EnsureReadable() override
		{
			if (InValues) { return true; }
			InValues = OutValues;
			return InValues ? true : false;
		}

		virtual bool IsWritable() override { return OutValues ? true : false; }
		virtual bool IsReadable() override { return InValues ? true : false; }
		virtual bool IsReadingFromWrite() override { return InValues == OutValues; }

		TSharedPtr<TArray<T>> GetInValues() { return InValues; }
		TSharedPtr<TArray<T>> GetOutValues() { return OutValues; }
		const FPCGMetadataAttribute<T>* GetTypedInAttribute() const { return TypedInAttribute; }
		FPCGMetadataAttribute<T>* GetTypedOutAttribute() { return TypedOutAttribute; }

		T& GetMutable(const int32 Index) { return *(OutValues->GetData() + Index); }
		const T& GetConst(const int32 Index) { return *(OutValues->GetData() + Index); }
		const T& Read(const int32 Index) const { return *(InValues->GetData() + Index); }

		void Set(const int32 Index, const T& Value) { *(OutValues->GetData() + Index) = Value; }

		virtual PCGEx::FAttributeIdentity GetTargetOutputIdentity() override
		{
			check(IsWritable() && OutAttribute)

			if (OutputsToDifferentName()) { return PCGEx::FAttributeIdentity(TargetOutputName, PCGEx::GetMetadataType<T>(), OutAttribute->AllowsInterpolation()); }
			return PCGEx::FAttributeIdentity(OutAttribute->Name, PCGEx::GetMetadataType<T>(), OutAttribute->AllowsInterpolation());
		}

		virtual bool OutputsToDifferentName() const override
		{
			// Don't consider None, @Source, @Last etc
			FString StrName = TargetOutputName.ToString();
			if (TargetOutputName.IsNone() || StrName.IsEmpty() || StrName.StartsWith(TEXT("@"))) { return false; }
			if (TypedOutAttribute) { return TypedOutAttribute->Name != TargetOutputName; }
			return false;
		}

	protected:
		void PrepareReadInternal(const bool bScoped, const FPCGMetadataAttributeBase* Attribute)
		{
			if (InValues) { return; }

			InValues = MakeShared<TArray<T>>();
			PCGEx::InitArray(InValues, Source->GetIn()->GetNumPoints());

			InAttribute = Attribute;
			TypedInAttribute = Attribute ? static_cast<const FPCGMetadataAttribute<T>*>(Attribute) : nullptr;

			bScopedBuffer = bScoped;
		}

		void PrepareWriteInternal(FPCGMetadataAttributeBase* Attribute, const T& InDefaultValue, const EBufferInit Init)
		{
			if (OutValues) { return; }

			OutValues = MakeShared<TArray<T>>();
			OutValues->Init(InDefaultValue, Source->GetOut()->GetNumPoints());

			OutAttribute = Attribute;
			TypedOutAttribute = Attribute ? static_cast<FPCGMetadataAttribute<T>*>(Attribute) : nullptr;
		}

	public:
		bool PrepareRead(const EIOSide InSide = EIOSide::In, const bool bScoped = false)
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

				if (InSide == EIOSide::In && OutValues && InValues == OutValues)
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

			if (InSide == EIOSide::Out)
			{
				// Reading from output
				check(OutValues)
				InValues = OutValues;
				return true;
			}

			UPCGMetadata* InMetadata = Source->GetIn()->Metadata;
			check(InMetadata)

			// 'template' spec required for clang on mac, and rider keeps removing it without the comment below.
			// ReSharper disable once CppRedundantTemplateKeyword
			TypedInAttribute = InMetadata->template GetConstTypedAttribute<T>(FullName);
			if (!TypedInAttribute) { return false; }

			TUniquePtr<const IPCGAttributeAccessor> InAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(TypedInAttribute, InMetadata);

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
				InAccessor->GetRange<T>(InRange, 0, *Source->GetInKeys());
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
				check(OutValues->Num() == Source->GetOut()->GetNumPoints())
				return true;
			}

			bIsNewOutput = !Source->GetOut()->Metadata->HasAttribute(FullName);
			TypedOutAttribute = Source->FindOrCreateAttribute(FullName, DefaultValue, bAllowInterpolation);

			if (!TypedOutAttribute)
			{
				return false;
			}

			TUniquePtr<IPCGAttributeAccessor> OutAccessor = PCGAttributeAccessorHelpers::CreateAccessor(TypedOutAttribute, Source->GetOut()->Metadata);

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
				TUniquePtr<FPCGAttributeAccessorKeysPointIndices> TempOutKeys = MakeUnique<FPCGAttributeAccessorKeysPointIndices>(Source->GetOut(), false);
				TArrayView<T> OutRange = MakeArrayView(OutValues->GetData(), OutValues->Num());
				if (!OutAccessor->GetRange<T>(OutRange, 0, *TempOutKeys.Get()))
				{
					// TODO : Log
				}
			};

			if (Init == EBufferInit::Inherit) { GrabExistingValues(); }
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
				// 'template' spec required for clang on mac, and rider keeps removing it without the comment below.
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

		virtual void Write(const bool bEnsureValidKeys = true) override
		{
			PCGEX_SHARED_CONTEXT_VOID(Source->GetContextHandle())

			if (!IsWritable() || !OutValues || !IsEnabled()) { return; }

			if (!Source->GetOut())
			{
				UE_LOG(LogPCGEx, Error, TEXT("Attempting to write data to an output that's not initialized!"));
				return;
			}

			TRACE_CPUPROFILER_EVENT_SCOPE(TBuffer::Write);

			// If we reach that point, data has been cleaned up and we're expected to create a new attribute here.

			if (OutputsToDifferentName())
			{
				// If we want to output to a different name, ensure the new attribute exists and hope we did proper validations beforehand

				// 'template' spec required for clang on mac, and rider keeps removing it without the comment below.
				// ReSharper disable once CppRedundantTemplateKeyword
				TypedOutAttribute = Source->GetOut()->Metadata->template FindOrCreateAttribute<T>(TargetOutputName, TypedOutAttribute->GetValueFromItemKey(PCGInvalidEntryKey), TypedOutAttribute->AllowsInterpolation());

				TUniquePtr<IPCGAttributeAccessor> OutAccessor = PCGAttributeAccessorHelpers::CreateAccessor(TypedOutAttribute, Source->GetOut()->Metadata);

				if (!OutAccessor.IsValid()) { return; }

				// Assume that if we write data, it's not to delete it.
				SharedContext.Get()->AddProtectedAttributeName(TargetOutputName);

				// Output value to fresh attribute	
				TArrayView<const T> View = MakeArrayView(OutValues->GetData(), OutValues->Num());
				OutAccessor->SetRange(View, 0, *Source->GetOutKeys(bEnsureValidKeys).Get());
			}
			else if (TypedOutAttribute)
			{
				// if we're not writing to a different name, then go through the usual flow

				TUniquePtr<IPCGAttributeAccessor> OutAccessor = PCGAttributeAccessorHelpers::CreateAccessor(TypedOutAttribute, Source->GetOut()->Metadata);
				if (!OutAccessor.IsValid()) { return; }

				// Assume that if we write data, it's not to delete it.
				SharedContext.Get()->AddProtectedAttributeName(TypedOutAttribute->Name);

				// Output value			
				TArrayView<const T> View = MakeArrayView(OutValues->GetData(), OutValues->Num());
				OutAccessor->SetRange<T>(View, 0, *Source->GetOutKeys(bEnsureValidKeys).Get());
			}
		}

		virtual void Fetch(const PCGExMT::FScope& Scope) override
		{
			if (!IsScoped() || bReadComplete || !IsEnabled()) { return; }
			if (InternalBroadcaster) { InternalBroadcaster->Fetch(*InValues, Scope); }

			if (TUniquePtr<const IPCGAttributeAccessor> InAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(TypedInAttribute, Source->GetIn()->Metadata);
				InAccessor.IsValid())
			{
				TArrayView<T> ReadRange = MakeArrayView(InValues->GetData() + Scope.Start, Scope.Count);
				InAccessor->GetRange<T>(ReadRange, Scope.Start, *Source->GetInKeys());
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

	class PCGEXTENDEDTOOLKIT_API FFacade : public TSharedFromThis<FFacade>
	{
		mutable FRWLock BufferLock;
		mutable FRWLock CloudLock;

	public:
		TSharedRef<FPointIO> Source;
		TArray<TSharedPtr<FBufferBase>> Buffers;
		TMap<uint64, TSharedPtr<FBufferBase>> BufferMap;
		TSharedPtr<PCGExGeo::FPointBoxCloud> Cloud;

		TMap<FName, FName> WritableRemap; // TODO : Manage remapping in the facade directly to remove the need for dummy attributes

		bool bSupportsScopedGet = false;

		int32 GetNum(const EIOSide InSide = EIOSide::In) const { return Source->GetNum(InSide); }

		TSharedPtr<FBufferBase> FindBuffer_Unsafe(const uint64 UID);
		TSharedPtr<FBufferBase> FindBuffer(const uint64 UID);
		TSharedPtr<FBufferBase> FindReadableAttributeBuffer(const FName InName);
		TSharedPtr<FBufferBase> FindWritableAttributeBuffer(const FName InName);


		explicit FFacade(const TSharedRef<FPointIO>& InSource):
			Source(InSource)
		{
			PCGEX_LOG_CTR(FFacade)
		}

		~FFacade() = default;

		bool IsDataValid(const EIOSide InSide) const { return Source->IsDataValid(InSide); }

		bool ShareSource(const FFacade* OtherManager) const { return this == OtherManager || OtherManager->Source == Source; }

		template <typename T>
		TSharedPtr<TBuffer<T>> FindBuffer_Unsafe(const FName FullName)
		{
			const TSharedPtr<FBufferBase>& Found = FindBuffer_Unsafe(BufferUID(FullName, PCGEx::GetMetadataType<T>()));
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

				NewBuffer = FindBuffer_Unsafe<T>(FullName);
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

		TSharedPtr<FBufferBase> GetWritable(EPCGMetadataTypes Type, const FPCGMetadataAttributeBase* InAttribute, EBufferInit Init);
		TSharedPtr<FBufferBase> GetWritable(EPCGMetadataTypes Type, const FName InName, EBufferInit Init);

#pragma endregion

#pragma region Readable

		template <typename T>
		TSharedPtr<TBuffer<T>> GetReadable(const FName InName, const EIOSide InSide = EIOSide::In)
		{
			TSharedPtr<TBuffer<T>> Buffer = GetBuffer<T>(InName);
			if (!Buffer->PrepareRead(InSide, false))
			{
				Flush(Buffer);
				return nullptr;
			}

			return Buffer;
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetScopedReadable(const FName InName, const EIOSide InSide = EIOSide::In)
		{
			if (!bSupportsScopedGet) { return GetReadable<T>(InName, InSide); }

			// Careful when reading from ESource::Out, make sure a writer already exists!!
			TSharedPtr<TBuffer<T>> Buffer = GetBuffer<T>(InName);
			if (!Buffer->PrepareRead(InSide, true))
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

		FPCGMetadataAttributeBase* FindMutableAttribute(const FName InName, const EIOSide InSide = EIOSide::In) const
		{
			const UPCGBasePointData* Data = Source->GetData(InSide);
			if (!Data) { return nullptr; }
			return Data->Metadata->GetMutableAttribute(InName);
		}

		const FPCGMetadataAttributeBase* FindConstAttribute(const FName InName, const EIOSide InSide = EIOSide::In) const
		{
			const UPCGBasePointData* Data = Source->GetData(InSide);
			if (!Data) { return nullptr; }
			return Data->Metadata->GetConstAttribute(InName);
		}

		template <typename T>
		FPCGMetadataAttribute<T>* FindMutableAttribute(const FName InName, const EIOSide InSide = EIOSide::In) const
		{
			const UPCGBasePointData* Data = Source->GetData(InSide);
			if (!Data) { return nullptr; }
			return Data->Metadata->GetMutableTypedAttribute<T>(InName);
		}

		template <typename T>
		const FPCGMetadataAttribute<T>* FindConstAttribute(const FName InName, const EIOSide InSide = EIOSide::In) const
		{
			const UPCGBasePointData* Data = Source->GetData(InSide);
			if (!Data) { return nullptr; }

			// 'template' spec required for clang on mac, and rider keeps removing it without the comment below.
			// ReSharper disable once CppRedundantTemplateKeyword
			return Data->Metadata->template GetConstTypedAttribute<T>(InName);
		}

		TSharedPtr<PCGExGeo::FPointBoxCloud> GetCloud(const EPCGExPointBoundsSource BoundsSource, const double Expansion = DBL_EPSILON);

		const UPCGBasePointData* GetData(const EIOSide InSide) const { return Source->GetData(InSide); }
		const UPCGBasePointData* GetIn() const { return Source->GetIn(); }
		UPCGBasePointData* GetOut() const { return Source->GetOut(); }

		void PrepareScopedReadable(const PCGEx::FAttributeIdentity& Identity);
		void PrepareScopedReadable(const TArray<PCGEx::FAttributeIdentity>& Identities);

		void MarkCurrentBuffersReadAsComplete();

		void Flush()
		{
			FWriteScopeLock WriteScopeLock(BufferLock);
			Buffers.Empty();
			BufferMap.Empty();
		}

		void Write(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const bool bEnsureValidKeys = true);
		FPlatformTypes::int32 WriteBuffersAsCallbacks(const TSharedPtr<PCGExMT::FTaskGroup>& TaskGroup);
		void WriteBuffers(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, PCGExMT::FCompletionCallback&& Callback);

		void Fetch(const PCGExMT::FScope& Scope)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FFacade::Fetch);
			for (const TSharedPtr<FBufferBase>& Buffer : Buffers) { Buffer->Fetch(Scope); }
		}

		FORCEINLINE FConstPoint GetInPoint(const int32 Index) const { return Source->GetInPoint(Index); }
		FORCEINLINE FMutablePoint GetOutPoint(const int32 Index) const { return Source->GetOutPoint(Index); }

		FORCEINLINE FScope GetInScope(const int32 Start, const int32 Count, const bool bInclusive = true) const { return Source->GetInScope(Start, Count, bInclusive); }
		FORCEINLINE FScope GetInScope(const PCGExMT::FScope& Scope) const { return Source->GetInScope(Scope); }
		FORCEINLINE FScope GetInFullScope() const { return Source->GetInFullScope(); }
		FORCEINLINE FScope GetInRange(const int32 Start, const int32 End, const bool bInclusive = true) const { return Source->GetInRange(Start, End, bInclusive); }

		FORCEINLINE FScope GetOutScope(const int32 Start, const int32 Count, const bool bInclusive = true) const { return Source->GetOutScope(Start, Count, bInclusive); }
		FORCEINLINE FScope GetOutScope(const PCGExMT::FScope& Scope) const { return Source->GetOutScope(Scope); }
		FORCEINLINE FScope GetOutFullScope() const { return Source->GetOutFullScope(); }
		FORCEINLINE FScope GetOutRange(const int32 Start, const int32 End, const bool bInclusive = true) const { return Source->GetOutRange(Start, End, bInclusive); }

	protected:
		template <typename Func>
		void ForEachWritable(Func&& Callback)
		{
			for (int i = 0; i < Buffers.Num(); i++)
			{
				const TSharedPtr<FBufferBase> Buffer = Buffers[i];
				if (!Buffer.IsValid() || !Buffer->IsWritable() || !Buffer->IsEnabled()) { continue; }
				Callback(Buffer);
			}
		}

		bool ValidateOutputsBeforeWriting() const;

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

	struct PCGEXTENDEDTOOLKIT_API FReadableBufferConfig
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

		bool Validate(FPCGExContext* InContext, const TSharedPtr<FFacade>& InFacade) const;
		void Fetch(const TSharedRef<FFacade>& InFacade, const PCGExMT::FScope& Scope) const;
		void Read(const TSharedRef<FFacade>& InFacade) const;
	};

	class PCGEXTENDEDTOOLKIT_API FFacadePreloader : public TSharedFromThis<FFacadePreloader>
	{
	protected:
		TWeakPtr<FFacade> InternalDataFacadePtr;

	public:
		TArray<FReadableBufferConfig> BufferConfigs;

		FFacadePreloader(const TSharedPtr<FFacade>& InDataFacade);

		bool IsEmpty() const { return BufferConfigs.IsEmpty(); }
		int32 Num() const { return BufferConfigs.Num(); }

		bool Validate(FPCGExContext* InContext, const TSharedPtr<FFacade>& InFacade) const;

		void Register(FPCGExContext* InContext, const PCGEx::FAttributeIdentity& InIdentity);

		void TryRegister(FPCGExContext* InContext, const FPCGAttributePropertyInputSelector& InSelector);

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

		void StartLoading(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const TSharedPtr<PCGExMT::FAsyncMultiHandle>& InParentHandle = nullptr);

	protected:
		void OnLoadingEnd() const;
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
		// 'template' spec required for clang on mac, and rider keeps removing it without the comment below.
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

	static UPCGBasePointData* GetMutablePointData(FPCGContext* Context, const FPCGTaggedData& Source)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Source.Data);
		if (!SpatialData) { return nullptr; }

		const UPCGBasePointData* PointData = SpatialData->ToPointData(Context);
		if (!PointData) { return nullptr; }

		return const_cast<UPCGBasePointData*>(PointData);
	}

#pragma endregion

	TSharedPtr<FFacade> TryGetSingleFacade(FPCGExContext* InContext, const FName InputPinLabel, bool bTransactional, const bool bThrowError);
	bool TryGetFacades(FPCGExContext* InContext, const FName InputPinLabel, TArray<TSharedPtr<FFacade>>& OutFacades, const bool bThrowError, const bool bIsTransactional = false);

	class PCGEXTENDEDTOOLKIT_API FWriteBufferTask final : public PCGExMT::FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FWriteTask)

		explicit FWriteBufferTask(const TSharedPtr<FBufferBase>& InBuffer, const bool InEnsureValidKeys = true)
			: FTask(), bEnsureValidKeys(InEnsureValidKeys), Buffer(InBuffer)
		{
		}

		bool bEnsureValidKeys = true;
		TSharedPtr<FBufferBase> Buffer;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override
		{
			if (!Buffer) { return; }
			Buffer->Write(bEnsureValidKeys);
		}
	};

	static void WriteBuffer(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const TSharedPtr<FBufferBase>& InBuffer, const bool InEnsureValidKeys = true)
	{
		if (!AsyncManager || !AsyncManager->IsAvailable()) { InBuffer->Write(InEnsureValidKeys); }
		PCGEX_LAUNCH(FWriteBufferTask, InBuffer, InEnsureValidKeys)
	}
}
