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
#include "PCGExDataHelpers.h"
#include "PCGExMT.h"
#include "Data/PCGPointData.h"

#pragma region DATA MACROS

#ifndef PCGEX_DATA_MACROS
#define PCGEX_DATA_MACROS

#define PCGEX_INIT_IO_VOID(_IO, _INIT) if (!_IO->InitializeOutput(_INIT)) { return; }
#define PCGEX_INIT_IO(_IO, _INIT) if (!_IO->InitializeOutput(_INIT)) { return false; }

#define PCGEX_CLEAR_IO_VOID(_IO) if (!_IO->InitializeOutput(PCGExData::EIOInit::NoInit)) { return; } _IO->Disable();
#define PCGEX_CLEAR_IO(_IO) if (!_IO->InitializeOutput(PCGExData::EIOInit::NoInit)) { return false; } _IO->Disable();

#endif
#pragma endregion

namespace PCGExGeo
{
	class FPointBoxCloud;
}

namespace PCGExData
{
	enum class EBufferInit : uint8
	{
		Inherit = 0,
		New,
	};

	enum class EDomainType : uint8
	{
		Unknown  = 0,
		Data     = 1,
		Elements = 2,
	};
	
#pragma region Buffers

	class FFacade;

	PCGEXTENDEDTOOLKIT_API
	uint64 BufferUID(const FPCGAttributeIdentifier& Identifier, const EPCGMetadataTypes Type);

	PCGEXTENDEDTOOLKIT_API
	FPCGAttributeIdentifier GetBufferIdentifierFromSelector(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData);

	class PCGEXTENDEDTOOLKIT_API IBuffer : public TSharedFromThis<IBuffer>
	{
		friend class FFacade;

	protected:
		mutable FRWLock BufferLock;

		EPCGMetadataTypes Type = EPCGMetadataTypes::Unknown;
		EDomainType UnderlyingDomain = EDomainType::Unknown;

		uint64 UID = 0;

		bool bIsNewOutput = false;
		std::atomic<bool> bIsEnabled{true}; // BUG : Need to have a better look at why we hang when this is false
		bool bReadComplete = false;

	public:
		FPCGAttributeIdentifier Identifier;

		bool IsEnabled() const { return bIsEnabled.load(std::memory_order_acquire); }
		void Disable() { bIsEnabled.store(false, std::memory_order_release); }
		void Enable() { bIsEnabled.store(true, std::memory_order_release); }

		const FPCGMetadataAttributeBase* InAttribute = nullptr;
		FPCGMetadataAttributeBase* OutAttribute = nullptr;

		int32 BufferIndex = -1;
		const TSharedRef<FPointIO> Source;

		IBuffer(const TSharedRef<FPointIO>& InSource, const FPCGAttributeIdentifier& InIdentifier):
			Identifier(InIdentifier), Source(InSource)
		{
			PCGEX_LOG_CTR(IBuffer)
		}

		FORCEINLINE uint64 GetUID() const { return UID; }
		FORCEINLINE EPCGMetadataTypes GetType() const { return Type; }
		FORCEINLINE EDomainType GetUnderlyingDomain() const { return UnderlyingDomain; }

		template <typename T>
		bool IsA() const { return Type == PCGEx::GetMetadataType<T>(); }

		virtual ~IBuffer();

		virtual bool EnsureReadable() = 0;
		virtual void Write(const bool bEnsureValidKeys = true) = 0;

		virtual void Fetch(const PCGExMT::FScope& Scope)
		{
		}

		virtual bool IsSparse() const { return false; }

		virtual bool IsWritable() = 0;
		virtual bool IsReadable() = 0;
		virtual bool ReadsFromOutput() = 0;

		virtual void Flush()
		{
		}

	protected:
		void SetType(const EPCGMetadataTypes InType);
	};

	template <typename T>
	class TBuffer : public IBuffer
	{
		friend class FFacade;

	protected:
		const FPCGMetadataAttribute<T>* TypedInAttribute = nullptr;
		FPCGMetadataAttribute<T>* TypedOutAttribute = nullptr;

	public:
		T Min = T{};
		T Max = T{};

		TBuffer(const TSharedRef<FPointIO>& InSource, const FPCGAttributeIdentifier& InIdentifier):
			IBuffer(InSource, InIdentifier)
		{
			SetType(PCGEx::GetMetadataType<T>());
		}

		const FPCGMetadataAttribute<T>* GetTypedInAttribute() const { return TypedInAttribute; }
		FPCGMetadataAttribute<T>* GetTypedOutAttribute() { return TypedOutAttribute; }

		// Unsafe read from input
		virtual const T& Read(const int32 Index) const = 0;

		// Unsafe read from output
		virtual const T& GetValue(const int32 Index) = 0;

		// Unsafe set value in output
		virtual void SetValue(const int32 Index, const T& Value) = 0;

		virtual bool InitForRead(const EIOSide InSide = EIOSide::In, const bool bScoped = false) = 0;
		virtual bool InitForBroadcast(const FPCGAttributePropertyInputSelector& InSelector, const bool bCaptureMinMax = false, const bool bScoped = false) = 0;
		virtual bool InitForWrite(const T& DefaultValue, bool bAllowInterpolation, EBufferInit Init = EBufferInit::Inherit) = 0;
		virtual bool InitForWrite(const EBufferInit Init = EBufferInit::Inherit) = 0;

		void DumpValues(TArray<T>& OutValues) const { for (int i = 0; i < OutValues.Num(); i++) { OutValues[i] = Read(i); } }
		void DumpValues(const TSharedPtr<TArray<T>>& OutValues) const { DumpValues(*OutValues.Get()); }
	};

#define PCGEX_USING_TBUFFER \
	friend class FFacade;\
	using TBuffer<T>::BufferLock;\
	using TBuffer<T>::Source;\
	using TBuffer<T>::Identifier;\
	using TBuffer<T>::InAttribute;\
	using TBuffer<T>::TypedInAttribute;\
	using TBuffer<T>::OutAttribute;\
	using TBuffer<T>::TypedOutAttribute;\
	using TBuffer<T>::bReadComplete;\
	using TBuffer<T>::IsEnabled;

	template <typename T>
	class TArrayBuffer : public TBuffer<T>
	{
		PCGEX_USING_TBUFFER

	protected:
		// Used to read from an attribute as another type
		TSharedPtr<PCGEx::TAttributeBroadcaster<T>> InternalBroadcaster;
		bool bSparseBuffer = false;

		TSharedPtr<TArray<T>> InValues;
		TSharedPtr<TArray<T>> OutValues;

	public:
		TArrayBuffer(const TSharedRef<FPointIO>& InSource, const FPCGAttributeIdentifier& InIdentifier):
			TBuffer<T>(InSource, InIdentifier)
		{
			check(InIdentifier.MetadataDomain.Flag != EPCGMetadataDomainFlag::Data)
			this->UnderlyingDomain = EDomainType::Elements;
		}

		virtual bool IsSparse() const override { return bSparseBuffer || InternalBroadcaster; }

		TSharedPtr<TArray<T>> GetInValues() { return InValues; }
		TSharedPtr<TArray<T>> GetOutValues() { return OutValues; }

		virtual bool IsWritable() override { return OutValues ? true : false; }
		virtual bool IsReadable() override { return InValues ? true : false; }
		virtual bool ReadsFromOutput() override { return InValues == OutValues; }

		virtual const T& Read(const int32 Index) const override { return *(InValues->GetData() + Index); }
		virtual const T& GetValue(const int32 Index) override { return *(OutValues->GetData() + Index); }
		virtual void SetValue(const int32 Index, const T& Value) override { *(OutValues->GetData() + Index) = Value; }

	protected:
		virtual void InitForReadInternal(const bool bScoped, const FPCGMetadataAttributeBase* Attribute)
		{
			if (InValues) { return; }

			InValues = MakeShared<TArray<T>>();
			PCGEx::InitArray(InValues, Source->GetIn()->GetNumPoints());

			InAttribute = Attribute;
			TypedInAttribute = Attribute ? static_cast<const FPCGMetadataAttribute<T>*>(Attribute) : nullptr;

			bSparseBuffer = bScoped;
		}

		virtual void InitForWriteInternal(FPCGMetadataAttributeBase* Attribute, const T& InDefaultValue, const EBufferInit Init)
		{
			if (OutValues) { return; }

			OutValues = MakeShared<TArray<T>>();
			OutValues->Init(InDefaultValue, Source->GetOut()->GetNumPoints());

			OutAttribute = Attribute;
			TypedOutAttribute = Attribute ? static_cast<FPCGMetadataAttribute<T>*>(Attribute) : nullptr;
		}

	public:
		virtual bool EnsureReadable() override
		{
			if (InValues) { return true; }
			InValues = OutValues;
			return InValues ? true : false;
		}

		virtual bool InitForRead(const EIOSide InSide = EIOSide::In, const bool bScoped = false) override
		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			if (InValues)
			{
				if (bSparseBuffer && !bScoped)
				{
					// Un-scoping reader.
					Fetch(PCGExMT::FScope(0, InValues->Num()));
					bReadComplete = true;
					bSparseBuffer = false;
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

			TypedInAttribute = PCGEx::TryGetConstAttribute<T>(Source->GetIn(), Identifier);
			if (!TypedInAttribute)
			{
				// Wrong type
				return false;
			}

			UPCGMetadata* InMetadata = Source->GetIn()->Metadata;
			check(InMetadata)

			TUniquePtr<const IPCGAttributeAccessor> InAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(TypedInAttribute, InMetadata);

			if (!InAccessor.IsValid())
			{
				TypedInAttribute = nullptr;
				InAccessor = nullptr;
				return false;
			}

			InitForReadInternal(bScoped, TypedInAttribute);

			if (!bSparseBuffer && !bReadComplete)
			{
				TArrayView<T> InRange = MakeArrayView(InValues->GetData(), InValues->Num());
				InAccessor->GetRange<T>(InRange, 0, *Source->GetInKeys());
				bReadComplete = true;
			}

			return true;
		}

		virtual bool InitForBroadcast(const FPCGAttributePropertyInputSelector& InSelector, const bool bCaptureMinMax = false, const bool bScoped = false) override
		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			if (InValues)
			{
				if (bSparseBuffer && !bScoped)
				{
					// Un-scoping reader.
					InternalBroadcaster->GrabAndDump(*InValues, bCaptureMinMax, this->Min, this->Max);
					bReadComplete = true;
					bSparseBuffer = false;
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

			InitForReadInternal(bScoped, InternalBroadcaster->GetAttribute());

			if (!bSparseBuffer && !bReadComplete)
			{
				InternalBroadcaster->GrabAndDump(*InValues, bCaptureMinMax, this->Min, this->Max);
				bReadComplete = true;
				InternalBroadcaster.Reset();
			}

			return true;
		}

		virtual bool InitForWrite(const T& DefaultValue, bool bAllowInterpolation, EBufferInit Init = EBufferInit::Inherit) override
		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			if (OutValues)
			{
				check(OutValues->Num() == Source->GetOut()->GetNumPoints())
				return true;
			}

			this->bIsNewOutput = !PCGEx::HasAttribute(Source->GetOut(), Identifier);

			if (this->bIsNewOutput) { TypedOutAttribute = Source->CreateAttribute(Identifier, DefaultValue, bAllowInterpolation); }
			else { TypedOutAttribute = Source->FindOrCreateAttribute(Identifier, DefaultValue, bAllowInterpolation); }

			if (!TypedOutAttribute) { return false; }

			TUniquePtr<IPCGAttributeAccessor> OutAccessor = PCGAttributeAccessorHelpers::CreateAccessor(TypedOutAttribute, Source->GetOut()->Metadata);

			if (!TypedOutAttribute || !OutAccessor.IsValid())
			{
				TypedOutAttribute = nullptr;
				return false;
			}

			InitForWriteInternal(TypedOutAttribute, DefaultValue, Init);

			const int32 ExistingEntryCount = TypedOutAttribute->GetNumberOfEntriesWithParents();
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

		virtual bool InitForWrite(const EBufferInit Init = EBufferInit::Inherit) override
		{
			{
				FWriteScopeLock WriteScopeLock(BufferLock);
				if (OutValues) { return true; }
			}

			if (const FPCGMetadataAttribute<T>* ExistingAttribute = PCGEx::TryGetConstAttribute<T>(Source->GetIn(), Identifier))
			{
				return InitForWrite(
					ExistingAttribute->GetValue(PCGDefaultValueKey),
					ExistingAttribute->AllowsInterpolation(),
					Init);
			}

			return InitForWrite(T{}, true, Init);
		}

		virtual void Write(const bool bEnsureValidKeys = true) override
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(TBuffer::Write);

			PCGEX_SHARED_CONTEXT_VOID(Source->GetContextHandle())

			if (!IsWritable() || !OutValues || !IsEnabled()) { return; }

			if (!Source->GetOut())
			{
				UE_LOG(LogPCGEx, Error, TEXT("Attempting to write data to an output that's not initialized!"));
				return;
			}

			if (!TypedOutAttribute) { return; }

			TUniquePtr<IPCGAttributeAccessor> OutAccessor = PCGAttributeAccessorHelpers::CreateAccessor(TypedOutAttribute, Source->GetOut()->Metadata);
			if (!OutAccessor.IsValid()) { return; }

			// Assume that if we write data, it's not to delete it.
			SharedContext.Get()->AddProtectedAttributeName(TypedOutAttribute->Name);

			// Output value			
			TArrayView<const T> View = MakeArrayView(OutValues->GetData(), OutValues->Num());
			OutAccessor->SetRange<T>(View, 0, *Source->GetOutKeys(bEnsureValidKeys).Get());
		}

		virtual void Fetch(const PCGExMT::FScope& Scope) override
		{
			if (!IsSparse() || bReadComplete || !IsEnabled()) { return; }
			if (InternalBroadcaster) { InternalBroadcaster->Fetch(*InValues, Scope); }

			if (TUniquePtr<const IPCGAttributeAccessor> InAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(TypedInAttribute, Source->GetIn()->Metadata);
				InAccessor.IsValid())
			{
				TArrayView<T> ReadRange = MakeArrayView(InValues->GetData() + Scope.Start, Scope.Count);
				InAccessor->GetRange<T>(ReadRange, Scope.Start, *Source->GetInKeys());
			}
		}

		virtual void Flush() override
		{
			InValues.Reset();
			OutValues.Reset();
			InternalBroadcaster.Reset();
		}
	};

	template <typename T>
	class TSingleValueBuffer : public TBuffer<T>
	{
		PCGEX_USING_TBUFFER

	protected:
		bool bReadInitialized = false;
		bool bWriteInitialized = false;
		bool bReadFromOutput = false;

		T InValue = T{};
		T OutValue = T{};

	public:
		virtual bool EnsureReadable() override
		{
			if (bReadInitialized) { return true; }

			InValue = OutValue;

			bReadFromOutput = true;
			bReadInitialized = bWriteInitialized;

			return bReadInitialized;
		}

		TSingleValueBuffer(const TSharedRef<FPointIO>& InSource, const FPCGAttributeIdentifier& InIdentifier):
			TBuffer<T>(InSource, InIdentifier)
		{
			check(InIdentifier.MetadataDomain.Flag == EPCGMetadataDomainFlag::Data)
			this->UnderlyingDomain = EDomainType::Data;
		}

		virtual bool IsWritable() override { return bWriteInitialized; }
		virtual bool IsReadable() override { return bReadInitialized; }
		virtual bool ReadsFromOutput() override { return bReadFromOutput; }

		virtual const T& Read(const int32 Index) const override
		{
			return InValue;
		}

		virtual const T& GetValue(const int32 Index) override
		{
			FReadScopeLock ReadScopeLock(BufferLock);
			return OutValue;
		}

		virtual void SetValue(const int32 Index, const T& Value) override
		{
			FWriteScopeLock WriteScopeLock(BufferLock);
			OutValue = Value;
			if (bReadFromOutput) { InValue = Value; }
		}

		virtual bool InitForRead(const EIOSide InSide = EIOSide::In, const bool bScoped = false) override
		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			if (bReadInitialized)
			{
				if (InSide == EIOSide::In && bWriteInitialized && bReadFromOutput)
				{
					check(false)
					// Out-source Reader was created before writer, this is bad?
				}
				else
				{
					return true;
				}
			}

			if (InSide == EIOSide::Out)
			{
				// Reading from output
				check(bWriteInitialized)

				bReadInitialized = bReadFromOutput = true;
				InValue = OutValue;

				return true;
			}

			TypedInAttribute = PCGEx::TryGetConstAttribute<T>(Source->GetIn(), Identifier);
			if (TypedInAttribute)
			{
				bReadInitialized = true;

				InAttribute = TypedInAttribute;
				InValue = PCGExDataHelpers::ReadDataValue(TypedInAttribute);
			}

			return bReadInitialized;
		}

		virtual bool InitForBroadcast(const FPCGAttributePropertyInputSelector& InSelector, const bool bCaptureMinMax = false, const bool bScoped = false) override
		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			if (bReadInitialized)
			{
				if (bWriteInitialized && bReadFromOutput)
				{
					check(false)
					// Out-source broadcaster was created before writer, this is bad?
				}
				else
				{
					return true;
				}
			}

			PCGEX_SHARED_CONTEXT(Source->GetContextHandle())
			bReadInitialized = PCGExDataHelpers::TryReadDataValue(SharedContext.Get(), Source->GetIn(), InSelector, InValue);

			return bReadInitialized;
		}

		virtual bool InitForWrite(const T& DefaultValue, bool bAllowInterpolation, EBufferInit Init = EBufferInit::Inherit) override
		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			if (bWriteInitialized) { return true; }

			this->bIsNewOutput = !PCGEx::HasAttribute(Source->GetOut(), Identifier);

			if (this->bIsNewOutput) { TypedOutAttribute = Source->CreateAttribute(Identifier, DefaultValue, bAllowInterpolation); }
			else { TypedOutAttribute = Source->FindOrCreateAttribute(Identifier, DefaultValue, bAllowInterpolation); }

			if (!TypedOutAttribute) { return false; }

			OutAttribute = TypedOutAttribute;
			bWriteInitialized = true;

			OutValue = DefaultValue;

			const int32 ExistingEntryCount = TypedOutAttribute->GetNumberOfEntriesWithParents();
			const bool bHasIn = Source->GetIn() ? true : false;

			auto GrabExistingValues = [&]()
			{
				OutValue = PCGExDataHelpers::ReadDataValue(TypedOutAttribute);
			};

			if (Init == EBufferInit::Inherit) { GrabExistingValues(); }
			else if (!bHasIn && ExistingEntryCount != 0) { GrabExistingValues(); }

			return bWriteInitialized;
		}

		virtual bool InitForWrite(const EBufferInit Init = EBufferInit::Inherit) override
		{
			{
				FWriteScopeLock WriteScopeLock(BufferLock);
				if (bWriteInitialized) { return true; }
			}

			if (const FPCGMetadataAttribute<T>* ExistingAttribute = PCGEx::TryGetConstAttribute<T>(Source->GetIn(), Identifier))
			{
				return InitForWrite(
					PCGExDataHelpers::ReadDataValue(ExistingAttribute),
					ExistingAttribute->AllowsInterpolation(),
					Init);
			}

			return InitForWrite(T{}, true, Init);
		}

		virtual void Write(const bool bEnsureValidKeys = true) override
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(TBuffer::Write);

			PCGEX_SHARED_CONTEXT_VOID(Source->GetContextHandle())

			if (!IsWritable() || !IsEnabled()) { return; }

			if (!Source->GetOut())
			{
				UE_LOG(LogPCGEx, Error, TEXT("Attempting to write data to an output that's not initialized!"));
				return;
			}

			if (!TypedOutAttribute) { return; }

			PCGExDataHelpers::SetDataValue(TypedOutAttribute, OutValue);
		}
	};

	class PCGEXTENDEDTOOLKIT_API FFacade : public TSharedFromThis<FFacade>
	{
		mutable FRWLock BufferLock;
		mutable FRWLock CloudLock;

	public:
		int32 Idx = -1;

		TSharedRef<FPointIO> Source;
		TArray<TSharedPtr<IBuffer>> Buffers;
		TMap<uint64, TSharedPtr<IBuffer>> BufferMap;
		TSharedPtr<PCGExGeo::FPointBoxCloud> Cloud;

		TMap<FName, FName> WritableRemap; // TODO : Manage remapping in the facade directly to remove the need for dummy attributes

		bool bSupportsScopedGet = false;

		int32 GetNum(const EIOSide InSide = EIOSide::In) const { return Source->GetNum(InSide); }

		TSharedPtr<IBuffer> FindBuffer_Unsafe(const uint64 UID);
		TSharedPtr<IBuffer> FindBuffer(const uint64 UID);
		TSharedPtr<IBuffer> FindReadableAttributeBuffer(const FPCGAttributeIdentifier& InIdentifier);
		TSharedPtr<IBuffer> FindWritableAttributeBuffer(const FPCGAttributeIdentifier& InIdentifier);

		EPCGPointNativeProperties GetAllocations() const { return Source->GetAllocations(); }

		explicit FFacade(const TSharedRef<FPointIO>& InSource):
			Source(InSource)
		{
			PCGEX_LOG_CTR(FFacade)
		}

		~FFacade() = default;

		bool IsDataValid(const EIOSide InSide) const { return Source->IsDataValid(InSide); }

		bool ShareSource(const FFacade* OtherManager) const { return this == OtherManager || OtherManager->Source == Source; }

		template <typename T>
		TSharedPtr<TBuffer<T>> FindBuffer_Unsafe(const FPCGAttributeIdentifier& InIdentifier)
		{
			const TSharedPtr<IBuffer>& Found = FindBuffer_Unsafe(BufferUID(InIdentifier, PCGEx::GetMetadataType<T>()));
			if (!Found) { return nullptr; }
			return StaticCastSharedPtr<TBuffer<T>>(Found);
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> FindBuffer(const FPCGAttributeIdentifier& InIdentifier)
		{
			const TSharedPtr<IBuffer> Found = FindBuffer(BufferUID(InIdentifier, PCGEx::GetMetadataType<T>()));
			if (!Found) { return nullptr; }
			return StaticCastSharedPtr<TBuffer<T>>(Found);
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetBuffer(const FPCGAttributeIdentifier& InIdentifier)
		{
			if (InIdentifier.MetadataDomain.Flag == EPCGMetadataDomainFlag::Invalid)
			{
				UE_LOG(LogPCGEx, Error, TEXT("GetBuffer : Invalid MetadataDomain for : '%s'"), *InIdentifier.Name.ToString());
				return nullptr;
			}

			TSharedPtr<TBuffer<T>> Buffer = FindBuffer<T>(InIdentifier);
			if (Buffer) { return Buffer; }

			{
				FWriteScopeLock WriteScopeLock(BufferLock);

				Buffer = FindBuffer_Unsafe<T>(InIdentifier);
				if (Buffer) { return Buffer; }

				if (InIdentifier.MetadataDomain.Flag == EPCGMetadataDomainFlag::Default ||
					InIdentifier.MetadataDomain.Flag == EPCGMetadataDomainFlag::Elements)
				{
					Buffer = MakeShared<TArrayBuffer<T>>(Source, InIdentifier);
				}
				else if (InIdentifier.MetadataDomain.Flag == EPCGMetadataDomainFlag::Data)
				{
					Buffer = MakeShared<TSingleValueBuffer<T>>(Source, InIdentifier);
				}
				else
				{
					UE_LOG(LogPCGEx, Error, TEXT("Attempting to create a buffer with unsupported domain."));
					return nullptr;
				}

				Buffer->BufferIndex = Buffers.Num();

				Buffers.Add(StaticCastSharedPtr<IBuffer>(Buffer));
				BufferMap.Add(Buffer->UID, Buffer);

				return Buffer;
			}
		}

#pragma region Writable

		template <typename T>
		TSharedPtr<TBuffer<T>> GetWritable(const FPCGAttributeIdentifier& InIdentifier, T DefaultValue, bool bAllowInterpolation, EBufferInit Init)
		{
			TSharedPtr<TBuffer<T>> Buffer = nullptr;

			if (InIdentifier.MetadataDomain.IsDefault())
			{
				Buffer = GetBuffer<T>(PCGEx::GetAttributeIdentifier(InIdentifier.Name, Source->GetOut()));
			}
			else
			{
				Buffer = GetBuffer<T>(InIdentifier);
			}

			if (!Buffer || !Buffer->InitForWrite(DefaultValue, bAllowInterpolation, Init)) { return nullptr; }
			return Buffer;
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetWritable(const FPCGMetadataAttribute<T>* InAttribute, EBufferInit Init)
		{
			return GetWritable(
				FPCGAttributeIdentifier(InAttribute->Name, InAttribute->GetMetadataDomain()->GetDomainID()),
				InAttribute->GetValue(PCGDefaultValueKey), InAttribute->AllowsInterpolation(), Init);
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetWritable(const FPCGAttributeIdentifier& InIdentifier, EBufferInit Init)
		{
			TSharedPtr<TBuffer<T>> Buffer = nullptr;

			if (InIdentifier.MetadataDomain.IsDefault())
			{
				// Identifier created from FName, need to sanitize it
				// We'll do so using a selector, this is expensive but quick and future proof
				Buffer = GetBuffer<T>(PCGEx::GetAttributeIdentifier(InIdentifier.Name, Source->GetOut()));
			}
			else
			{
				Buffer = GetBuffer<T>(InIdentifier);
			}

			if (!Buffer || !Buffer->InitForWrite(Init)) { return nullptr; }
			return Buffer;
		}

		TSharedPtr<IBuffer> GetWritable(EPCGMetadataTypes Type, const FPCGMetadataAttributeBase* InAttribute, EBufferInit Init);
		TSharedPtr<IBuffer> GetWritable(EPCGMetadataTypes Type, const FName InName, EBufferInit Init);

#pragma endregion

#pragma region Readable

		template <typename T>
		TSharedPtr<TBuffer<T>> GetReadable(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide = EIOSide::In, const bool bSupportScoped = false)
		{
			TSharedPtr<TBuffer<T>> Buffer = nullptr;

			if (InIdentifier.MetadataDomain.IsDefault())
			{
				// Identifier created from FName, need to sanitize it
				// We'll do so using a selector, this is expensive but quick and future proof
				Buffer = GetBuffer<T>(PCGEx::GetAttributeIdentifier(InIdentifier.Name, Source->GetData(InSide)));
			}
			else
			{
				Buffer = GetBuffer<T>(InIdentifier);
			}

			if (!Buffer || !Buffer->InitForRead(InSide, bSupportsScopedGet ? bSupportScoped : false))
			{
				Flush(Buffer);
				return nullptr;
			}

			return Buffer;
		}

		TSharedPtr<IBuffer> GetReadable(const PCGEx::FAttributeIdentity& Identity, const EIOSide InSide = EIOSide::In, const bool bSupportScoped = false);

#pragma endregion

#pragma region Broadcasters

		template <typename T>
		TSharedPtr<TBuffer<T>> GetBroadcaster(const FPCGAttributePropertyInputSelector& InSelector, const bool bSupportScoped = false, const bool bCaptureMinMax = false)
		{
			// Build a proper identifier from the selector
			// We'll use it to get a unique buffer ID as well as domain, which is conditional to finding the right buffer class to use

			const FPCGAttributeIdentifier Identifier = GetBufferIdentifierFromSelector(InSelector, Source->GetIn());
			if (Identifier.MetadataDomain.Flag == EPCGMetadataDomainFlag::Invalid)
			{
				UE_LOG(LogPCGEx, Error, TEXT("GetBroadcaster : Invalid domain with '%s'"), *Identifier.Name.ToString());
				return nullptr;
			}

			TSharedPtr<TBuffer<T>> Buffer = GetBuffer<T>(Identifier);
			if (!Buffer || !Buffer->InitForBroadcast(InSelector, bCaptureMinMax, bCaptureMinMax || !bSupportsScopedGet ? false : bSupportScoped))
			{
				Flush(Buffer);
				return nullptr;
			}

			return Buffer;
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetBroadcaster(const FName InName, const bool bSupportScoped = false, const bool bCaptureMinMax = false)
		{
			// Create a selector from the identifier.
			// This is a bit backward but the user may have added domain prefixes to the name such as @Data.
			FPCGAttributePropertyInputSelector Selector = FPCGAttributePropertyInputSelector();
			Selector.Update(InName.ToString());

			return GetBroadcaster<T>(Selector, bSupportScoped, bCaptureMinMax);
		}

#pragma endregion

		FPCGMetadataAttributeBase* FindMutableAttribute(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide = EIOSide::In) const
		{
			return Source->FindMutableAttribute(InIdentifier, InSide);
		}

		const FPCGMetadataAttributeBase* FindConstAttribute(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide = EIOSide::In) const
		{
			return Source->FindConstAttribute(InIdentifier, InSide);
		}

		template <typename T>
		FPCGMetadataAttribute<T>* FindMutableAttribute(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide = EIOSide::In) const
		{
			return Source->FindMutableAttribute<T>(InIdentifier, InSide);
		}

		template <typename T>
		const FPCGMetadataAttribute<T>* FindConstAttribute(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide = EIOSide::In) const
		{
			return Source->FindConstAttribute<T>(InIdentifier, InSide);
		}

		TSharedPtr<PCGExGeo::FPointBoxCloud> GetCloud(const EPCGExPointBoundsSource BoundsSource, const double Expansion = DBL_EPSILON);

		const UPCGBasePointData* GetData(const EIOSide InSide) const { return Source->GetData(InSide); }
		const UPCGBasePointData* GetIn() const { return Source->GetIn(); }
		UPCGBasePointData* GetOut() const { return Source->GetOut(); }

		void CreateReadables(const TArray<PCGEx::FAttributeIdentity>& Identities, const bool bWantsScoped = true);

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
		int32 WriteSynchronous(const bool bEnsureValidKeys = true);
		void WriteFastest(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const bool bEnsureValidKeys = true);

		void Fetch(const PCGExMT::FScope& Scope)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FFacade::Fetch);
			for (const TSharedPtr<IBuffer>& Buffer : Buffers) { Buffer->Fetch(Scope); }
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
				const TSharedPtr<IBuffer> Buffer = Buffers[i];
				if (!Buffer.IsValid() || !Buffer->IsWritable() || !Buffer->IsEnabled()) { continue; }
				Callback(Buffer);
			}
		}

		bool ValidateOutputsBeforeWriting() const;

		void Flush(const TSharedPtr<IBuffer>& Buffer)
		{
			if (Buffer)
			{
				FWriteScopeLock WriteScopeLock(BufferLock);
				Buffers.RemoveAt(Buffer->BufferIndex);
				BufferMap.Remove(Buffer->GetUID());
				for (int i = 0; i < Buffers.Num(); i++) { Buffers[i].Get()->BufferIndex = i; }
			}
		}
	};

#pragma endregion

#pragma region Data Marking

	template <typename T>
	static FPCGMetadataAttribute<T>* WriteMark(const TSharedRef<FPointIO>& PointIO, const FName MarkID, T MarkValue)
	{
		const FPCGAttributeIdentifier Identifier = PCGEx::GetAttributeIdentifier(MarkID, PointIO->GetOut());
		PointIO->DeleteAttribute(Identifier);
		FPCGMetadataAttribute<T>* Mark = PointIO->CreateAttribute<T>(Identifier, MarkValue);
		PCGExDataHelpers::SetDataValue(Mark, MarkValue);
		return Mark;
	}


	template <typename T>
	static bool TryReadMark(UPCGMetadata* Metadata, const FPCGAttributeIdentifier& MarkID, T& OutMark)
	{
		// 'template' spec required for clang on mac, and rider keeps removing it without the comment below.
		// ReSharper disable once CppRedundantTemplateKeyword
		const FPCGMetadataAttribute<T>* Mark = PCGEx::TryGetConstAttribute<T>(Metadata, MarkID);
		if (!Mark) { return false; }
		OutMark = PCGExDataHelpers::ReadDataValue(Mark);
		return true;
	}

	template <typename T>
	static bool TryReadMark(const TSharedRef<FPointIO>& PointIO, const FName MarkID, T& OutMark)
	{
		const FPCGAttributeIdentifier Identifier = PCGEx::GetAttributeIdentifier(MarkID, PointIO->GetIn());
		return TryReadMark(PointIO->GetIn() ? PointIO->GetIn()->Metadata : PointIO->GetOut()->Metadata, Identifier, OutMark);
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

		explicit FWriteBufferTask(const TSharedPtr<IBuffer>& InBuffer, const bool InEnsureValidKeys = true)
			: FTask(), bEnsureValidKeys(InEnsureValidKeys), Buffer(InBuffer)
		{
		}

		bool bEnsureValidKeys = true;
		TSharedPtr<IBuffer> Buffer;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override
		{
			if (!Buffer) { return; }
			Buffer->Write(bEnsureValidKeys);
		}
	};

	static void WriteBuffer(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const TSharedPtr<IBuffer>& InBuffer, const bool InEnsureValidKeys = true)
	{
		if (InBuffer->GetUnderlyingDomain() == EDomainType::Data)
		{
			// Immediately write data values
			// Note : let's hope this won't put async in limbo 
			InBuffer->Write(InEnsureValidKeys);
		}
		else
		{
			if (!AsyncManager || !AsyncManager->IsAvailable()) { InBuffer->Write(InEnsureValidKeys); }
			PCGEX_LAUNCH(FWriteBufferTask, InBuffer, InEnsureValidKeys)
		}
	}
}
