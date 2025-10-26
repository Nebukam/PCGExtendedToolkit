﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExData.h"

#include "PCGExMT.h"
#include "PCGExGlobalSettings.h"
#include "PCGExH.h"
#include "PCGExHelpers.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGExValueHash.h"
#include "Data/PCGPointData.h"
#include "Geometry/PCGExGeoPointBox.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGCustomAccessor.h"

namespace PCGExData
{
#pragma region Buffers

	uint64 BufferUID(const FPCGAttributeIdentifier& Identifier, const EPCGMetadataTypes Type)
	{
		EPCGMetadataDomainFlag SaneFlagForUID = Identifier.MetadataDomain.Flag;
		if (SaneFlagForUID == EPCGMetadataDomainFlag::Default) { SaneFlagForUID = EPCGMetadataDomainFlag::Elements; }
		return PCGEx::H64(HashCombine(GetTypeHash(Identifier.Name), GetTypeHash(SaneFlagForUID)), static_cast<int32>(Type));
	}

	FPCGAttributeIdentifier GetBufferIdentifierFromSelector(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData)
	{
		// This return an identifier suitable to be used for data facade

		FPCGAttributeIdentifier Identifier;

		if (!InData) { return FPCGAttributeIdentifier(PCGEx::InvalidName, EPCGMetadataDomainFlag::Invalid); }

		FPCGAttributePropertyInputSelector FixedSelector = InSelector.CopyAndFixLast(InData);

		if (InSelector.GetExtraNames().IsEmpty()) { Identifier.Name = FixedSelector.GetName(); }
		else { Identifier.Name = FName(FixedSelector.GetName().ToString() + TEXT(".") + FString::Join(FixedSelector.GetExtraNames(), TEXT("."))); }

		Identifier.MetadataDomain = InData->GetMetadataDomainIDFromSelector(FixedSelector);

		return Identifier;
	}

	IBuffer::IBuffer(const TSharedRef<FPointIO>& InSource, const FPCGAttributeIdentifier& InIdentifier)
		: Identifier(InIdentifier), Source(InSource)
	{
	}

	IBuffer::~IBuffer()
	{
		Flush();
	}

	template <typename T>
	bool IBuffer::IsA() const { return Type == PCGEx::GetMetadataType<T>(); }

#define PCGEX_TPL(_TYPE, _NAME, ...) \
template PCGEXTENDEDTOOLKIT_API bool IBuffer::IsA<_TYPE>() const;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

	void IBuffer::SetType(const EPCGMetadataTypes InType)
	{
		Type = InType;
		UID = BufferUID(Identifier, InType);
	}

	template <typename T>
	TBuffer<T>::TBuffer(const TSharedRef<FPointIO>& InSource, const FPCGAttributeIdentifier& InIdentifier)
		: IBuffer(InSource, InIdentifier)
	{
		SetType(PCGEx::GetMetadataType<T>());
	}

	template <typename T>
	const FPCGMetadataAttribute<T>* TBuffer<T>::GetTypedInAttribute() const { return TypedInAttribute; }

	template <typename T>
	FPCGMetadataAttribute<T>* TBuffer<T>::GetTypedOutAttribute() const { return TypedOutAttribute; }

	template <typename T>
	uint32 TBuffer<T>::ReadValueHash(const int32 Index) { return PCGExBlend::ValueHash(Read(Index)); }

	template <typename T>
	uint32 TBuffer<T>::GetValueHash(const int32 Index) { return PCGExBlend::ValueHash(GetValue(Index)); }

	template <typename T>
	void TBuffer<T>::DumpValues(TArray<T>& OutValues) const { for (int i = 0; i < OutValues.Num(); i++) { OutValues[i] = Read(i); } }

	template <typename T>
	void TBuffer<T>::DumpValues(const TSharedPtr<TArray<T>>& OutValues) const { DumpValues(*OutValues.Get()); }

	template <typename T>
	TArrayBuffer<T>::TArrayBuffer(const TSharedRef<FPointIO>& InSource, const FPCGAttributeIdentifier& InIdentifier):
		TBuffer<T>(InSource, InIdentifier)
	{
		check(InIdentifier.MetadataDomain.Flag != EPCGMetadataDomainFlag::Data)
		this->UnderlyingDomain = EDomainType::Elements;
	}

	template <typename T>
	TSharedPtr<TArray<T>> TArrayBuffer<T>::GetInValues() { return InValues; }

	template <typename T>
	TSharedPtr<TArray<T>> TArrayBuffer<T>::GetOutValues() { return OutValues; }

	template <typename T>
	int32 TArrayBuffer<T>::GetNumValues(const EIOSide InSide)
	{
		if (InSide == EIOSide::In)
		{
			return InValues ? InValues->Num() : -1;
		}
		return OutValues ? OutValues->Num() : -1;
	}

	template <typename T>
	bool TArrayBuffer<T>::IsWritable() { return OutValues ? true : false; }

	template <typename T>
	bool TArrayBuffer<T>::IsReadable() { return InValues ? true : false; }

	template <typename T>
	bool TArrayBuffer<T>::ReadsFromOutput() { return InValues == OutValues; }

	template <typename T>
	const T& TArrayBuffer<T>::Read(const int32 Index) const { return *(InValues->GetData() + Index); }

	template <typename T>
	const T& TArrayBuffer<T>::GetValue(const int32 Index) { return *(OutValues->GetData() + Index); }

	template <typename T>
	void TArrayBuffer<T>::SetValue(const int32 Index, const T& Value) { *(OutValues->GetData() + Index) = Value; }

	template <typename T>
	void TArrayBuffer<T>::InitForReadInternal(const bool bScoped, const FPCGMetadataAttributeBase* Attribute)
	{
		if (InValues) { return; }

		InValues = MakeShared<TArray<T>>();
		PCGEx::InitArray(InValues, Source->GetIn()->GetNumPoints());

		InAttribute = Attribute;
		TypedInAttribute = Attribute ? static_cast<const FPCGMetadataAttribute<T>*>(Attribute) : nullptr;

		bSparseBuffer = bScoped;
	}

	template <typename T>
	void TArrayBuffer<T>::InitForWriteInternal(FPCGMetadataAttributeBase* Attribute, const T& InDefaultValue, const EBufferInit Init)
	{
		if (OutValues) { return; }

		OutValues = MakeShared<TArray<T>>();
		OutValues->Init(InDefaultValue, Source->GetOut()->GetNumPoints());

		OutAttribute = Attribute;
		TypedOutAttribute = Attribute ? static_cast<FPCGMetadataAttribute<T>*>(Attribute) : nullptr;
	}

	template <typename T>
	bool TArrayBuffer<T>::EnsureReadable()
	{
		if (InValues) { return true; }
		InValues = OutValues;
		return InValues ? true : false;
	}

	template <typename T>
	bool TArrayBuffer<T>::InitForRead(const EIOSide InSide, const bool bScoped)
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

	template <typename T>
	bool TArrayBuffer<T>::InitForBroadcast(const FPCGAttributePropertyInputSelector& InSelector, const bool bCaptureMinMax, const bool bScoped)
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

	template <typename T>
	bool TArrayBuffer<T>::InitForWrite(const T& DefaultValue, bool bAllowInterpolation, EBufferInit Init)
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

	template <typename T>
	bool TArrayBuffer<T>::InitForWrite(const EBufferInit Init)
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

	template <typename T>
	void TArrayBuffer<T>::Write(const bool bEnsureValidKeys)
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

		if (this->bResetWithFirstValue)
		{
			TypedOutAttribute->Reset();
			TypedOutAttribute->SetDefaultValue(*OutValues->GetData());
			return;
		}

		TUniquePtr<IPCGAttributeAccessor> OutAccessor = PCGAttributeAccessorHelpers::CreateAccessor(TypedOutAttribute, Source->GetOut()->Metadata);
		if (!OutAccessor.IsValid()) { return; }

		// Assume that if we write data, it's not to delete it.
		SharedContext.Get()->AddProtectedAttributeName(TypedOutAttribute->Name);

		// Output value			
		TArrayView<const T> View = MakeArrayView(OutValues->GetData(), OutValues->Num());
		OutAccessor->SetRange<T>(View, 0, *Source->GetOutKeys(bEnsureValidKeys).Get());
	}

	template <typename T>
	void TArrayBuffer<T>::Fetch(const PCGExMT::FScope& Scope)
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

	template <typename T>
	void TArrayBuffer<T>::Flush()
	{
		InValues.Reset();
		OutValues.Reset();
		InternalBroadcaster.Reset();
	}

	template <typename T>
	int32 TSingleValueBuffer<T>::GetNumValues(const EIOSide InSide)
	{
		return 1;
	}

	template <typename T>
	bool TSingleValueBuffer<T>::EnsureReadable()
	{
		if (bReadInitialized) { return true; }

		InValue = OutValue;

		bReadFromOutput = true;
		bReadInitialized = bWriteInitialized;

		return bReadInitialized;
	}

	template <typename T>
	TSingleValueBuffer<T>::TSingleValueBuffer(const TSharedRef<FPointIO>& InSource, const FPCGAttributeIdentifier& InIdentifier)
		: TBuffer<T>(InSource, InIdentifier)
	{
		check(InIdentifier.MetadataDomain.Flag == EPCGMetadataDomainFlag::Data)
		this->UnderlyingDomain = EDomainType::Data;
	}

	template <typename T>
	bool TSingleValueBuffer<T>::IsWritable() { return bWriteInitialized; }

	template <typename T>
	bool TSingleValueBuffer<T>::IsReadable() { return bReadInitialized; }

	template <typename T>
	bool TSingleValueBuffer<T>::ReadsFromOutput() { return bReadFromOutput; }

	template <typename T>
	const T& TSingleValueBuffer<T>::Read(const int32 Index) const
	{
		return InValue;
	}

	template <typename T>
	const T& TSingleValueBuffer<T>::GetValue(const int32 Index)
	{
		FReadScopeLock ReadScopeLock(BufferLock);
		return OutValue;
	}

	template <typename T>
	void TSingleValueBuffer<T>::SetValue(const int32 Index, const T& Value)
	{
		FWriteScopeLock WriteScopeLock(BufferLock);
		OutValue = Value;
		if (bReadFromOutput) { InValue = Value; }
	}

	template <typename T>
	bool TSingleValueBuffer<T>::InitForRead(const EIOSide InSide, const bool bScoped)
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

	template <typename T>
	bool TSingleValueBuffer<T>::InitForBroadcast(const FPCGAttributePropertyInputSelector& InSelector, const bool bCaptureMinMax, const bool bScoped)
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

	template <typename T>
	bool TSingleValueBuffer<T>::InitForWrite(const T& DefaultValue, bool bAllowInterpolation, EBufferInit Init)
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

	template <typename T>
	bool TSingleValueBuffer<T>::InitForWrite(const EBufferInit Init)
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

	template <typename T>
	void TSingleValueBuffer<T>::Write(const bool bEnsureValidKeys)
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

	int32 FFacade::GetNum(const EIOSide InSide) const { return Source->GetNum(InSide); }

	TSharedPtr<IBuffer> FFacade::FindBuffer_Unsafe(const uint64 UID)
	{
		TSharedPtr<IBuffer>* Found = BufferMap.Find(UID);
		if (!Found) { return nullptr; }
		return *Found;
	}

	TSharedPtr<IBuffer> FFacade::FindBuffer(const uint64 UID)
	{
		FReadScopeLock ReadScopeLock(BufferLock);
		return FindBuffer_Unsafe(UID);
	}

	TSharedPtr<IBuffer> FFacade::FindReadableAttributeBuffer(const FPCGAttributeIdentifier& InIdentifier)
	{
		FReadScopeLock ReadScopeLock(BufferLock);
		for (const TSharedPtr<IBuffer>& Buffer : Buffers)
		{
			if (!Buffer->IsReadable()) { continue; }
			if (Buffer->InAttribute && Buffer->InAttribute->Name == InIdentifier.Name) { return Buffer; }
		}
		return nullptr;
	}

	TSharedPtr<IBuffer> FFacade::FindWritableAttributeBuffer(const FPCGAttributeIdentifier& InIdentifier)
	{
		FReadScopeLock ReadScopeLock(BufferLock);
		for (const TSharedPtr<IBuffer>& Buffer : Buffers)
		{
			if (!Buffer->IsWritable()) { continue; }
			if (Buffer->Identifier == InIdentifier) { return Buffer; }
		}
		return nullptr;
	}

	EPCGPointNativeProperties FFacade::GetAllocations() const { return Source->GetAllocations(); }

	FPCGExContext* FFacade::GetContext() const
	{
		const FPCGContext::FSharedContext<FPCGExContext> SharedContext(Source->GetContextHandle());
		return SharedContext.Get();
	}

	FFacade::FFacade(const TSharedRef<FPointIO>& InSource)
		: Source(InSource)
	{
	}

	bool FFacade::IsDataValid(const EIOSide InSide) const { return Source->IsDataValid(InSide); }

	bool FFacade::ShareSource(const FFacade* OtherManager) const { return this == OtherManager || OtherManager->Source == Source; }

	template <typename T>
	TSharedPtr<TBuffer<T>> FFacade::FindBuffer_Unsafe(const FPCGAttributeIdentifier& InIdentifier)
	{
		const TSharedPtr<IBuffer>& Found = FindBuffer_Unsafe(BufferUID(InIdentifier, PCGEx::GetMetadataType<T>()));
		if (!Found) { return nullptr; }
		return StaticCastSharedPtr<TBuffer<T>>(Found);
	}

	template <typename T>
	TSharedPtr<TBuffer<T>> FFacade::FindBuffer(const FPCGAttributeIdentifier& InIdentifier)
	{
		const TSharedPtr<IBuffer> Found = FindBuffer(BufferUID(InIdentifier, PCGEx::GetMetadataType<T>()));
		if (!Found) { return nullptr; }
		return StaticCastSharedPtr<TBuffer<T>>(Found);
	}

	template <typename T>
	TSharedPtr<TBuffer<T>> FFacade::GetBuffer(const FPCGAttributeIdentifier& InIdentifier)
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


	template <typename T>
	TSharedPtr<TBuffer<T>> FFacade::GetWritable(const FPCGAttributeIdentifier& InIdentifier, T DefaultValue, bool bAllowInterpolation, EBufferInit Init)
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
	TSharedPtr<TBuffer<T>> FFacade::GetWritable(const FPCGMetadataAttribute<T>* InAttribute, EBufferInit Init)
	{
		return GetWritable(
			FPCGAttributeIdentifier(InAttribute->Name, InAttribute->GetMetadataDomain()->GetDomainID()),
			InAttribute->GetValue(PCGDefaultValueKey), InAttribute->AllowsInterpolation(), Init);
	}

	template <typename T>
	TSharedPtr<TBuffer<T>> FFacade::GetWritable(const FPCGAttributeIdentifier& InIdentifier, EBufferInit Init)
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

	template <typename T>
	TSharedPtr<TBuffer<T>> FFacade::GetReadable(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide, const bool bSupportScoped)
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

	template <typename T>
	TSharedPtr<TBuffer<T>> FFacade::GetBroadcaster(const FPCGAttributePropertyInputSelector& InSelector, const bool bSupportScoped, const bool bCaptureMinMax)
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
	TSharedPtr<TBuffer<T>> FFacade::GetBroadcaster(const FName InName, const bool bSupportScoped, const bool bCaptureMinMax)
	{
		// Create a selector from the identifier.
		// This is a bit backward but the user may have added domain prefixes to the name such as @Data.
		FPCGAttributePropertyInputSelector Selector = FPCGAttributePropertyInputSelector();
		Selector.Update(InName.ToString());

		return GetBroadcaster<T>(Selector, bSupportScoped, bCaptureMinMax);
	}

	template <typename T>
	FPCGMetadataAttribute<T>* FFacade::FindMutableAttribute(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide) const
	{
		return Source->FindMutableAttribute<T>(InIdentifier, InSide);
	}

	template <typename T>
	const FPCGMetadataAttribute<T>* FFacade::FindConstAttribute(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide) const
	{
		return Source->FindConstAttribute<T>(InIdentifier, InSide);
	}

#define PCGEX_TPL(_TYPE, _NAME, ...) \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<TBuffer<_TYPE>> FFacade::FindBuffer_Unsafe<_TYPE>(const FPCGAttributeIdentifier& InIdentifier); \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<TBuffer<_TYPE>> FFacade::FindBuffer<_TYPE>(const FPCGAttributeIdentifier& InIdentifier); \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<TBuffer<_TYPE>> FFacade::GetBuffer<_TYPE>(const FPCGAttributeIdentifier& InIdentifier); \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<TBuffer<_TYPE>> FFacade::GetWritable<_TYPE>(const FPCGAttributeIdentifier& InIdentifier, _TYPE DefaultValue, bool bAllowInterpolation, EBufferInit Init); \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<TBuffer<_TYPE>> FFacade::GetWritable<_TYPE>(const FPCGMetadataAttribute<_TYPE>* InAttribute, EBufferInit Init); \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<TBuffer<_TYPE>> FFacade::GetWritable<_TYPE>(const FPCGAttributeIdentifier& InIdentifier, EBufferInit Init); \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<TBuffer<_TYPE>> FFacade::GetReadable<_TYPE>(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide, const bool bSupportScoped); \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<TBuffer<_TYPE>> FFacade::GetBroadcaster<_TYPE>(const FPCGAttributePropertyInputSelector& InSelector, const bool bSupportScoped, const bool bCaptureMinMax); \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<TBuffer<_TYPE>> FFacade::GetBroadcaster<_TYPE>(const FName InName, const bool bSupportScoped, const bool bCaptureMinMax); \
template PCGEXTENDEDTOOLKIT_API FPCGMetadataAttribute<_TYPE>* FFacade::FindMutableAttribute<_TYPE>(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide) const; \
template PCGEXTENDEDTOOLKIT_API const FPCGMetadataAttribute<_TYPE>* FFacade::FindConstAttribute<_TYPE>(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide) const;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

	TSharedPtr<IBuffer> FFacade::GetWritable(const EPCGMetadataTypes Type, const FPCGMetadataAttributeBase* InAttribute, EBufferInit Init)
	{
#define PCGEX_TYPED_WRITABLE(_TYPE, _ID, ...) case EPCGMetadataTypes::_ID: return GetWritable<_TYPE>(static_cast<const FPCGMetadataAttribute<_TYPE>*>(InAttribute), Init);
		switch (Type)
		{
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TYPED_WRITABLE)
		default: return nullptr;
		}
#undef PCGEX_TYPED_WRITABLE
	}

	TSharedPtr<IBuffer> FFacade::GetWritable(const EPCGMetadataTypes Type, const FName InName, EBufferInit Init)
	{
#define PCGEX_TYPED_WRITABLE(_TYPE, _ID, ...) case EPCGMetadataTypes::_ID: return GetWritable<_TYPE>(InName, Init);
		switch (Type)
		{
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TYPED_WRITABLE)
		default: return nullptr;
		}
#undef PCGEX_TYPED_WRITABLE
	}

	TSharedPtr<IBuffer> FFacade::GetReadable(const PCGEx::FAttributeIdentity& Identity, const EIOSide InSide, const bool bSupportScoped)
	{
		TSharedPtr<IBuffer> Buffer = nullptr;

#define PCGEX_TYPED_EXEC(_TYPE, _NAME) Buffer = GetReadable<_TYPE>(Identity.Identifier, InSide, bSupportScoped);
		PCGEX_EXECUTEWITHRIGHTTYPE(Identity.UnderlyingType, PCGEX_TYPED_EXEC)
#undef PCGEX_TYPED_EXEC

		return Buffer;
	}

	TSharedPtr<IBuffer> FFacade::GetDefaultReadable(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide, const bool bSupportScoped)
	{
		TSharedPtr<IBuffer> Buffer = nullptr;
		const FPCGMetadataAttributeBase* RawAttribute = Source->FindConstAttribute(InIdentifier, InSide);

		if (!RawAttribute) { return nullptr; }

#define PCGEX_TYPED_EXEC(_TYPE, _NAME) Buffer = Buffer = GetReadable<_TYPE>(InIdentifier, InSide, bSupportScoped);
		PCGEX_EXECUTEWITHRIGHTTYPE(static_cast<EPCGMetadataTypes>(RawAttribute->GetTypeId()), PCGEX_TYPED_EXEC)
#undef PCGEX_TYPED_EXEC

		return Buffer;
	}

#pragma endregion

#pragma region externalization

#define PCGEX_TPL(_TYPE, _NAME, ...)\
template class PCGEXTENDEDTOOLKIT_API TBuffer<_TYPE>;\
template class PCGEXTENDEDTOOLKIT_API TArrayBuffer<_TYPE>;\
template class PCGEXTENDEDTOOLKIT_API TSingleValueBuffer<_TYPE>;

	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)

#undef PCGEX_TPL

#pragma endregion

#pragma region FFacade

	FPCGMetadataAttributeBase* FFacade::FindMutableAttribute(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide) const

	{
		return Source->FindMutableAttribute(InIdentifier, InSide);
	}

	const FPCGMetadataAttributeBase* FFacade::FindConstAttribute(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide) const

	{
		return Source->FindConstAttribute(InIdentifier, InSide);
	}

	TSharedPtr<PCGExGeo::FPointBoxCloud> FFacade::GetCloud(const EPCGExPointBoundsSource BoundsSource, const double Expansion)
	{
		FWriteScopeLock WriteScopeLock(CloudLock);

		if (Cloud) { return Cloud; }

		Cloud = MakeShared<PCGExGeo::FPointBoxCloud>(GetIn(), BoundsSource, Expansion);
		Cloud->Idx = Idx;
		return Cloud;
	}

	const UPCGBasePointData* FFacade::GetData(const EIOSide InSide) const { return Source->GetData(InSide); }

	const UPCGBasePointData* FFacade::GetIn() const { return Source->GetIn(); }

	UPCGBasePointData* FFacade::GetOut() const { return Source->GetOut(); }

	void FFacade::CreateReadables(const TArray<PCGEx::FAttributeIdentity>& Identities, const bool bWantsScoped)
	{
		for (const PCGEx::FAttributeIdentity& Identity : Identities) { GetReadable(Identity, EIOSide::In, bWantsScoped); }
	}

	void FFacade::MarkCurrentBuffersReadAsComplete()
	{
		for (const TSharedPtr<IBuffer>& Buffer : Buffers)
		{
			if (!Buffer.IsValid() || !Buffer->IsReadable()) { continue; }
			Buffer->bReadComplete = true;
		}
	}

	void FFacade::Flush()
	{
		FWriteScopeLock WriteScopeLock(BufferLock);
		Buffers.Empty();
		BufferMap.Empty();
	}

	void FFacade::Write(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const bool bEnsureValidKeys)
	{
		if (!AsyncManager || !AsyncManager->IsAvailable() || !Source->GetOut()) { return; }

		if (ValidateOutputsBeforeWriting())
		{
			if (bEnsureValidKeys) { Source->GetOutKeys(true); }

			{
				FWriteScopeLock WriteScopeLock(BufferLock);

				for (int i = 0; i < Buffers.Num(); i++)
				{
					const TSharedPtr<IBuffer> Buffer = Buffers[i];
					if (!Buffer.IsValid() || !Buffer->IsWritable() || !Buffer->IsEnabled()) { continue; }
					WriteBuffer(AsyncManager, Buffer, false);
				}
			}
		}

		Flush();
	}

	int32 FFacade::WriteBuffersAsCallbacks(const TSharedPtr<PCGExMT::FTaskGroup>& TaskGroup)
	{
		// !!! Requires manual flush !!!

		if (!TaskGroup || !ValidateOutputsBeforeWriting())
		{
			Flush();
			return -1;
		}

		int32 WritableCount = 0;
		Source->GetOutKeys(true);

		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			for (int i = 0; i < Buffers.Num(); i++)
			{
				const TSharedPtr<IBuffer> Buffer = Buffers[i];
				if (!Buffer.IsValid() || !Buffer->IsWritable() || !Buffer->IsEnabled()) { continue; }

				TaskGroup->AddSimpleCallback([BufferRef = Buffer]() { BufferRef->Write(); });
				WritableCount++;
			}
		}

		return WritableCount;
	}

	void FFacade::WriteBuffers(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, PCGExMT::FCompletionCallback&& Callback)
	{
		if (!ValidateOutputsBeforeWriting())
		{
			Flush();
			return;
		}

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, WriteBuffersWithCallback);
		WriteBuffersWithCallback->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE, Callback]()
			{
				PCGEX_ASYNC_THIS
				This->Flush();
				Callback();
			};

		if (const int32 WritableCount = WriteBuffersAsCallbacks(WriteBuffersWithCallback); WritableCount <= 0)
		{
			// -1 is fail so no callback
			if (WritableCount == 0) { Callback(); }
			return;
		}

		WriteBuffersWithCallback->StartSimpleCallbacks();
	}

	int32 FFacade::WriteSynchronous(const bool bEnsureValidKeys)
	{
		if (!Source->GetOut()) { return -1; }

		int32 WritableCount = 0;

		if (ValidateOutputsBeforeWriting())
		{
			if (bEnsureValidKeys) { Source->GetOutKeys(true); }

			{
				FWriteScopeLock WriteScopeLock(BufferLock);

				for (int i = 0; i < Buffers.Num(); i++)
				{
					const TSharedPtr<IBuffer> Buffer = Buffers[i];
					if (!Buffer.IsValid() || !Buffer->IsWritable() || !Buffer->IsEnabled()) { continue; }
					Buffer->Write(false);
					WritableCount++;
				}
			}
		}

		Flush();
		return WritableCount;
	}

	void FFacade::WriteFastest(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const bool bEnsureValidKeys)
	{
		if (!Source->GetOut()) { return; }

		if (Source->GetNum(EIOSide::Out) < GetDefault<UPCGExGlobalSettings>()->SmallPointsSize) { WriteSynchronous(bEnsureValidKeys); }
		else { Write(AsyncManager, bEnsureValidKeys); }
	}

	void FFacade::Fetch(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FFacade::Fetch);
		for (const TSharedPtr<IBuffer>& Buffer : Buffers) { Buffer->Fetch(Scope); }
	}

	FConstPoint FFacade::GetInPoint(const int32 Index) const { return Source->GetInPoint(Index); }
	FMutablePoint FFacade::GetOutPoint(const int32 Index) const { return Source->GetOutPoint(Index); }

	FScope FFacade::GetInScope(const int32 Start, const int32 Count, const bool bInclusive) const { return Source->GetInScope(Start, Count, bInclusive); }
	FScope FFacade::GetInScope(const PCGExMT::FScope& Scope) const { return Source->GetInScope(Scope); }
	FScope FFacade::GetInFullScope() const { return Source->GetInFullScope(); }
	FScope FFacade::GetInRange(const int32 Start, const int32 End, const bool bInclusive) const { return Source->GetInRange(Start, End, bInclusive); }

	FScope FFacade::GetOutScope(const int32 Start, const int32 Count, const bool bInclusive) const { return Source->GetOutScope(Start, Count, bInclusive); }
	FScope FFacade::GetOutScope(const PCGExMT::FScope& Scope) const { return Source->GetOutScope(Scope); }
	FScope FFacade::GetOutFullScope() const { return Source->GetOutFullScope(); }
	FScope FFacade::GetOutRange(const int32 Start, const int32 End, const bool bInclusive) const { return Source->GetOutRange(Start, End, bInclusive); }

	bool FFacade::ValidateOutputsBeforeWriting() const
	{
		PCGEX_SHARED_CONTEXT(Source->GetContextHandle())
		FPCGExContext* Context = SharedContext.Get();

		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			TSet<FPCGAttributeIdentifier> UniqueOutputs;
			for (int i = 0; i < Buffers.Num(); i++)
			{
				const TSharedPtr<IBuffer> Buffer = Buffers[i];
				if (!Buffer.IsValid() || !Buffer->IsWritable() || !Buffer->IsEnabled()) { continue; }

				FPCGAttributeIdentifier Identifier = Buffer->Identifier;
				bool bAlreadySet = false;
				UniqueOutputs.Add(Identifier, &bAlreadySet);

				if (bAlreadySet)
				{
					PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("Attribute \"{0}\" is written to at least twice by different buffers."), FText::FromName(Identifier.Name)));
					return false;
				}
			}
		}

		return true;
	}

	void FFacade::Flush(const TSharedPtr<IBuffer>& Buffer)
	{
		if (!Buffer || !Buffer.IsValid()) { return; }

		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			if (Buffers.IsValidIndex(Buffer->BufferIndex)) { Buffers.RemoveAt(Buffer->BufferIndex); }
			BufferMap.Remove(Buffer->UID);

			int32 WriteIndex = 0;
			for (int i = 0; i < Buffers.Num(); i++)
			{
				TSharedPtr<IBuffer> TempBuffer = Buffers[i];

				if (!TempBuffer || !TempBuffer.IsValid()) { continue; }
				TempBuffer->BufferIndex = WriteIndex++;
				Buffers[TempBuffer->BufferIndex] = TempBuffer;
			}
		}
	}

	template <typename T>
	FPCGMetadataAttribute<T>* WriteMark(UPCGData* InData, const FPCGAttributeIdentifier& MarkID, T MarkValue)
	{
		UPCGMetadata* Metadata = InData->MutableMetadata();

		if (!Metadata) { return nullptr; }

		Metadata->DeleteAttribute(MarkID);
		FPCGMetadataAttribute<T>* Mark = Metadata->CreateAttribute<T>(MarkID, MarkValue, true, true);
		PCGExDataHelpers::SetDataValue(Mark, MarkValue);
		return Mark;
	}

	template <typename T>
	FPCGMetadataAttribute<T>* WriteMark(const TSharedRef<FPointIO>& PointIO, const FName MarkID, T MarkValue)
	{
		const FPCGAttributeIdentifier Identifier = PCGEx::GetAttributeIdentifier(MarkID, PointIO->GetOut());
		return WriteMark<T>(PointIO->GetMutableData(EIOSide::Out), Identifier, MarkValue);
	}

	template <typename T>
	bool TryReadMark(UPCGMetadata* Metadata, const FPCGAttributeIdentifier& MarkID, T& OutMark)
	{
		// 'template' spec required for clang on mac, and rider keeps removing it without the comment below.
		// ReSharper disable once CppRedundantTemplateKeyword
		const FPCGMetadataAttribute<T>* Mark = PCGEx::TryGetConstAttribute<T>(Metadata, MarkID);
		if (!Mark) { return false; }
		OutMark = PCGExDataHelpers::ReadDataValue(Mark);
		return true;
	}

	template <typename T>
	bool TryReadMark(const TSharedRef<FPointIO>& PointIO, const FName MarkID, T& OutMark)
	{
		const FPCGAttributeIdentifier Identifier = PCGEx::GetAttributeIdentifier(MarkID, PointIO->GetIn());
		return TryReadMark(PointIO->GetIn() ? PointIO->GetIn()->Metadata : PointIO->GetOut()->Metadata, Identifier, OutMark);
	}

#define PCGEX_TPL(_TYPE, _NAME, ...)\
template PCGEXTENDEDTOOLKIT_API FPCGMetadataAttribute<_TYPE>* WriteMark(UPCGData* InData, const FPCGAttributeIdentifier& MarkID, _TYPE MarkValue); \
template PCGEXTENDEDTOOLKIT_API FPCGMetadataAttribute<_TYPE>* WriteMark(const TSharedRef<FPointIO>& PointIO, const FName MarkID, _TYPE MarkValue); \
template PCGEXTENDEDTOOLKIT_API bool TryReadMark<_TYPE>(UPCGMetadata* Metadata, const FPCGAttributeIdentifier& MarkID, _TYPE& OutMark); \
template PCGEXTENDEDTOOLKIT_API bool TryReadMark<_TYPE>(const TSharedRef<FPointIO>& PointIO, const FName MarkID, _TYPE& OutMark);

	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)

#undef PCGEX_TPL

	void WriteId(const TSharedRef<FPointIO>& PointIO, const FName IdName, const int64 Id)
	{
		PointIO->Tags->Set<int64>(IdName.ToString(), Id);
		if (PointIO->GetOut()) { WriteMark(PointIO, IdName, Id); }
	}

	UPCGBasePointData* GetMutablePointData(FPCGContext* Context, const FPCGTaggedData& Source)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Source.Data);
		if (!SpatialData) { return nullptr; }

		const UPCGBasePointData* PointData = SpatialData->ToPointData(Context);
		if (!PointData) { return nullptr; }

		return const_cast<UPCGBasePointData*>(PointData);
	}

	void FWriteBufferTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		if (!Buffer) { return; }
		Buffer->Write(bEnsureValidKeys);
	}

	void WriteBuffer(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const TSharedPtr<IBuffer>& InBuffer, const bool InEnsureValidKeys)
	{
		if (InBuffer->GetUnderlyingDomain() == EDomainType::Data || InBuffer->bResetWithFirstValue)
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

#pragma endregion
}
