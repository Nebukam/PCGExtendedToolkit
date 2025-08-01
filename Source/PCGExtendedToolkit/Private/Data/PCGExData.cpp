// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExData.h"

#include "PCGExPointsMT.h"
#include "Geometry/PCGExGeoPointBox.h"

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

	IBuffer::~IBuffer()
	{
		Flush();
	}

	void IBuffer::SetType(const EPCGMetadataTypes InType)
	{
		Type = InType;
		UID = BufferUID(Identifier, InType);
	}

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

	FPCGExContext* FFacade::GetContext() const
	{
		const FPCGContext::FSharedContext<FPCGExContext> SharedContext(Source->GetContextHandle());
		return SharedContext.Get();
	}

	FFacade::FFacade(const TSharedRef<FPointIO>& InSource)
		: Source(InSource)
	{
		PCGEX_LOG_CTR(FFacade)
	}


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
		PCGEx::ExecuteWithRightType(
			Identity.UnderlyingType, [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				Buffer = GetReadable<T>(Identity.Identifier, InSide, bSupportScoped);
			});

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

	TSharedPtr<PCGExGeo::FPointBoxCloud> FFacade::GetCloud(const EPCGExPointBoundsSource BoundsSource, const double Expansion)
	{
		FWriteScopeLock WriteScopeLock(CloudLock);

		if (Cloud) { return Cloud; }

		Cloud = MakeShared<PCGExGeo::FPointBoxCloud>(GetIn(), BoundsSource, Expansion);
		return Cloud;
	}

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
