// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExData.h"

#include "PCGExPointsMT.h"
#include "Geometry/PCGExGeoPointBox.h"

namespace PCGExData
{
#pragma region cache

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

	void IBuffer::SetTargetOutputName(const FName InName)
	{
		TargetOutputName = InName;
	}

	PCGEx::FAttributeIdentity IBuffer::GetTargetOutputIdentity()
	{
		check(IsWritable() && OutAttribute)
		return PCGEx::FAttributeIdentity(
			OutputsToDifferentName() ? TargetOutputName : OutAttribute->Name,
			Type, OutAttribute->AllowsInterpolation());
	}

	bool IBuffer::OutputsToDifferentName() const
	{
		// Don't consider None, @Source, @Last etc
		FString StrName = TargetOutputName.ToString();
		if (TargetOutputName.IsNone() || StrName.IsEmpty() || StrName.StartsWith(TEXT("@"))) { return false; }
		if (OutAttribute) { return OutAttribute->Name != TargetOutputName; }
		return false;
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

	void FFacade::Write(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const bool bEnsureValidKeys)
	{
		if (!AsyncManager || !AsyncManager->IsAvailable() || !Source->GetOut()) { return; }

		//UE_LOG(LogPCGEx, Warning, TEXT("{%lld} Facade -> Write"), AsyncManager->Context->GetInputSettings<UPCGSettings>()->UID)

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

	bool FFacade::ValidateOutputsBeforeWriting() const
	{
		PCGEX_SHARED_CONTEXT(Source->GetContextHandle())
		FPCGExContext* Context = SharedContext.Get();

		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			TSet<FName> UniqueOutputs;
			//TSet<FName> DeprecatedOutputs;

			for (int i = 0; i < Buffers.Num(); i++)
			{
				const TSharedPtr<IBuffer> Buffer = Buffers[i];
				if (!Buffer.IsValid() || !Buffer->IsWritable() || !Buffer->IsEnabled()) { continue; }

				PCGEx::FAttributeIdentity Identity = Buffer->GetTargetOutputIdentity();
				bool bAlreadySet = false;
				UniqueOutputs.Add(Identity.Identifier.Name, &bAlreadySet);

				if (bAlreadySet)
				{
					PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("Attribute \"{0}\" is used at target output at least twice by different sources."), FText::FromName(Identity.Identifier.Name)));
					return false;
				}

				if (Buffer->OutputsToDifferentName())
				{
					// Get rid of new attributes that are being redirected at the time of writing
					// This is not elegant, but it's while waiting for a proper refactor

					if (Buffer->bIsNewOutput && Buffer->OutAttribute)
					{
						// Note : there will be a problem if an output an attribute to a different name a different type than the original one
						// i.e, From@A<double>, To@B<FVector>
						// Although this should never be an issue due to templating
						//DeprecatedOutputs.Add(Buffer->OutAttribute->Name);
						Source->DeleteAttribute(Buffer->OutAttribute->Name);
					}
				}
			}

			// Make sure we don't deprecate any output that will be written to ?
			// UniqueOutputs > DeprecatedOutputs
		}

		return true;
	}

#pragma endregion

	bool FReadableBufferConfig::Validate(FPCGExContext* InContext, const TSharedPtr<FFacade>& InFacade) const
	{
		return true;
	}

	void FReadableBufferConfig::Fetch(const TSharedRef<FFacade>& InFacade, const PCGExMT::FScope& Scope)
	{
		TSharedPtr<IBuffer> Reader = nullptr;

		{
			FReadScopeLock ReadScopeLock(ReaderLock);
			Reader = WeakReader.Pin();
		}

		if (!Reader)
		{
			PCGEx::ExecuteWithRightType(
				Identity.UnderlyingType, [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					FWriteScopeLock WriteScopeLock(ReaderLock);

					switch (Mode)
					{
					case EBufferPreloadType::RawAttribute:
						Reader = InFacade->GetReadable<T>(Identity.Identifier, EIOSide::In, true);
						break;
					case EBufferPreloadType::BroadcastFromName:
						Reader = InFacade->GetBroadcaster<T>(Identity.Identifier.Name, true);
						break;
					case EBufferPreloadType::BroadcastFromSelector:
						Reader = InFacade->GetBroadcaster<T>(Selector, true);
						break;
					}

					WeakReader = Reader;
				});
		}

		Reader->Fetch(Scope);
	}

	void FReadableBufferConfig::Read(const TSharedRef<FFacade>& InFacade) const
	{
		PCGEx::ExecuteWithRightType(
			Identity.UnderlyingType, [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				TSharedPtr<TBuffer<T>> Reader = nullptr;
				switch (Mode)
				{
				case EBufferPreloadType::RawAttribute:
					Reader = InFacade->GetReadable<T>(Identity.Identifier);
					break;
				case EBufferPreloadType::BroadcastFromName:
					Reader = InFacade->GetBroadcaster<T>(Identity.Identifier.Name);
					break;
				case EBufferPreloadType::BroadcastFromSelector:
					Reader = InFacade->GetBroadcaster<T>(Selector);
					break;
				}
			});
	}

	FFacadePreloader::FFacadePreloader(const TSharedPtr<FFacade>& InDataFacade)
		: InternalDataFacadePtr(InDataFacade)
	{
	}

	TSharedPtr<FFacade> FFacadePreloader::GetDataFacade() const
	{
		return InternalDataFacadePtr.Pin();
	}

	bool FFacadePreloader::Validate(FPCGExContext* InContext, const TSharedPtr<FFacade>& InFacade) const
	{
		if (BufferConfigs.IsEmpty()) { return true; }
		for (const FReadableBufferConfig& Config : BufferConfigs) { if (!Config.Validate(InContext, InFacade)) { return false; } }
		return true;
	}

	void FFacadePreloader::Register(FPCGExContext* InContext, const PCGEx::FAttributeIdentity& InIdentity)
	{
		for (const FReadableBufferConfig& ExistingConfig : BufferConfigs)
		{
			if (ExistingConfig.Identity == InIdentity) { return; }
		}

		BufferConfigs.Emplace(InIdentity.Identifier.Name, InIdentity.UnderlyingType);
	}

	void FFacadePreloader::TryRegister(FPCGExContext* InContext, const FPCGAttributePropertyInputSelector& InSelector)
	{
		TSharedPtr<FFacade> SourceFacade = GetDataFacade();
		if (!SourceFacade) { return; }

		PCGEx::FAttributeIdentity Identity;
		if (PCGEx::FAttributeIdentity::Get(SourceFacade->GetIn(), InSelector, Identity))
		{
			Register(InContext, Identity);
		}
	}

	void FFacadePreloader::Fetch(const TSharedRef<FFacade>& InFacade, const PCGExMT::FScope& Scope)
	{
		for (FReadableBufferConfig& ExistingConfig : BufferConfigs) { ExistingConfig.Fetch(InFacade, Scope); }
	}

	void FFacadePreloader::Read(const TSharedRef<FFacade>& InFacade, const int32 ConfigIndex) const
	{
		BufferConfigs[ConfigIndex].Read(InFacade);
	}

	void FFacadePreloader::StartLoading(
		const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager,
		const TSharedPtr<PCGExMT::FAsyncMultiHandle>& InParentHandle)
	{
		TSharedPtr<FFacade> SourceFacade = GetDataFacade();
		if (!SourceFacade) { return; }

		if (!IsEmpty())
		{
			if (!Validate(AsyncManager->GetContext(), SourceFacade))
			{
				InternalDataFacadePtr.Reset();
				OnLoadingEnd();
				return;
			}

			PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, PrefetchAttributesTask)
			PrefetchAttributesTask->SetParent(InParentHandle);

			PrefetchAttributesTask->OnCompleteCallback =
				[PCGEX_ASYNC_THIS_CAPTURE]()
				{
					PCGEX_ASYNC_THIS
					This->OnLoadingEnd();
				};

			if (SourceFacade->bSupportsScopedGet)
			{
				PrefetchAttributesTask->OnSubLoopStartCallback =
					[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
					{
						PCGEX_ASYNC_THIS
						if (const TSharedPtr<FFacade> InternalFacade = This->GetDataFacade())
						{
							This->Fetch(InternalFacade.ToSharedRef(), Scope);
						}
					};

				PrefetchAttributesTask->StartSubLoops(SourceFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
			}
			else
			{
				PrefetchAttributesTask->OnIterationCallback =
					[PCGEX_ASYNC_THIS_CAPTURE](const int32 Index, const PCGExMT::FScope& Scope)
					{
						PCGEX_ASYNC_THIS
						if (const TSharedPtr<FFacade> InternalFacade = This->GetDataFacade())
						{
							This->Read(InternalFacade.ToSharedRef(), Index);
						}
					};

				PrefetchAttributesTask->StartIterations(Num(), 1);
			}
		}
		else
		{
			OnLoadingEnd();
		}
	}

	void FFacadePreloader::OnLoadingEnd() const
	{
		if (TSharedPtr<FFacade> InternalFacade = GetDataFacade()) { InternalFacade->MarkCurrentBuffersReadAsComplete(); }
		if (OnCompleteCallback) { OnCompleteCallback(); }
	}
}
