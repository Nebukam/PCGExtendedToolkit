// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExDataPreloader.h"
#include "Data/PCGExData.h"

namespace PCGExData
{
	bool FReadableBufferConfig::Validate(FPCGExContext* InContext, const TSharedPtr<FFacade>& InFacade) const
	{
		return true;
	}

	void FReadableBufferConfig::Fetch(const TSharedRef<FFacade>& InFacade, const PCGExMT::FScope& Scope)
	{
		if (Status == -1) { return; }

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

			if (!Reader)
			{
				Status = -1;
				return;
			}
			else
			{
				Status = 1;
			}
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

	bool FFacadePreloader::Validate(FPCGExContext* InContext) const
	{
		if (BufferConfigs.IsEmpty()) { return true; }
		const TSharedPtr<FFacade> InDataFacade = InternalDataFacadePtr.Pin();
		for (const FReadableBufferConfig& Config : BufferConfigs) { if (!Config.Validate(InContext, InDataFacade)) { return false; } }
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
			if (!Validate(AsyncManager->GetContext()))
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

	void FFacadePreloader::OnLoadingEnd()
	{
		if (bLoaded) { return; }
		bLoaded = true;
		if (TSharedPtr<FFacade> InternalFacade = GetDataFacade()) { InternalFacade->MarkCurrentBuffersReadAsComplete(); }
		if (OnCompleteCallback) { OnCompleteCallback(); }
	}

	FMultiFacadePreloader::FMultiFacadePreloader(const TArray<TSharedPtr<FFacade>>& InDataFacades)
	{
		Preloaders.Reserve(InDataFacades.Num());
		for (const TSharedPtr<FFacade>& InDataFacade : InDataFacades)
		{
			TSharedPtr<FFacadePreloader> Preloader = MakeShared<FFacadePreloader>(InDataFacade);
			Preloaders.Add(Preloader);
		}
	}

	FMultiFacadePreloader::FMultiFacadePreloader(const TArray<TSharedRef<FFacade>>& InDataFacades)
	{
		Preloaders.Reserve(InDataFacades.Num());
		for (const TSharedRef<FFacade>& InDataFacade : InDataFacades)
		{
			TSharedPtr<FFacadePreloader> Preloader = MakeShared<FFacadePreloader>(InDataFacade);
			Preloaders.Add(Preloader);
		}
	}

	void FMultiFacadePreloader::ForEach(PreloaderItCallback&& It)
	{
		for (const TSharedPtr<FFacadePreloader>& Preloader : Preloaders) { It(*Preloader.Get()); }
	}

	bool FMultiFacadePreloader::Validate(FPCGExContext* InContext)
	{
		for (const TSharedPtr<FFacadePreloader>& Preloader : Preloaders) { if (!Preloader->Validate(InContext)) { return false; } }
		return true;
	}

	void FMultiFacadePreloader::StartLoading(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const TSharedPtr<PCGExMT::FAsyncMultiHandle>& InParentHandle)
	{
		for (const TSharedPtr<FFacadePreloader>& Preloader : Preloaders)
		{
			Preloader->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->OnSubloadComplete();
			};
		}
		for (const TSharedPtr<FFacadePreloader>& Preloader : Preloaders) { Preloader->StartLoading(AsyncManager, InParentHandle); }
	}

	void FMultiFacadePreloader::OnSubloadComplete()
	{
		for (const TSharedPtr<FFacadePreloader>& Preloader : Preloaders) { if (!Preloader->IsLoaded()) { return; } }
		OnLoadingEnd();
	}

	void FMultiFacadePreloader::OnLoadingEnd() const
	{
		if (OnCompleteCallback) { OnCompleteCallback(); }
	}
}
