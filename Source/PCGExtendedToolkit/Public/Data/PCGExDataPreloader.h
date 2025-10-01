// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExAttributeHelpers.h"
#include "UObject/Object.h"

#include "PCGExData.h"
#include "PCGExMT.h"

namespace PCGExData
{
	PCGEX_CTX_STATE(State_PreloadingData)

	enum class EBufferPreloadType : uint8
	{
		RawAttribute = 0,
		BroadcastFromName,
		BroadcastFromSelector,
	};

	struct PCGEXTENDEDTOOLKIT_API FReadableBufferConfig
	{
	protected:
		FRWLock ReaderLock;
		int8 Status = 0;
		TWeakPtr<IBuffer> WeakReader;

	public:
		EBufferPreloadType Mode = EBufferPreloadType::RawAttribute;
		FPCGAttributePropertyInputSelector Selector;
		PCGEx::FAttributeIdentity Identity;

		FReadableBufferConfig(const FReadableBufferConfig& Other);
		explicit FReadableBufferConfig(const PCGEx::FAttributeIdentity& InIdentity, EBufferPreloadType InMode = EBufferPreloadType::RawAttribute);
		FReadableBufferConfig(const FName InName, const EPCGMetadataTypes InUnderlyingType, EBufferPreloadType InMode = EBufferPreloadType::RawAttribute);
		FReadableBufferConfig(const FPCGAttributePropertyInputSelector& InSelector, const EPCGMetadataTypes InUnderlyingType);

		bool Validate(FPCGExContext* InContext, const TSharedPtr<FFacade>& InFacade) const;
		void Fetch(const TSharedRef<FFacade>& InFacade, const PCGExMT::FScope& Scope);
		void Read(const TSharedRef<FFacade>& InFacade) const;
	};

	class PCGEXTENDEDTOOLKIT_API FFacadePreloader : public TSharedFromThis<FFacadePreloader>
	{
	protected:
		TWeakPtr<FPCGContextHandle> WeakHandle;
		TWeakPtr<FFacade> InternalDataFacadePtr;
		bool bLoaded = false;

	public:
		TArray<FReadableBufferConfig> BufferConfigs;

		FFacadePreloader(const TSharedPtr<FFacade>& InDataFacade);

		TSharedPtr<FFacade> GetDataFacade() const;

		bool IsEmpty() const { return BufferConfigs.IsEmpty(); }
		int32 Num() const { return BufferConfigs.Num(); }

		bool Validate(FPCGExContext* InContext) const;

		void Register(FPCGExContext* InContext, const PCGEx::FAttributeIdentity& InIdentity);

		void TryRegister(FPCGExContext* InContext, const FPCGAttributePropertyInputSelector& InSelector);

		template <typename T>
		void Register(FPCGExContext* InContext, const FPCGAttributePropertyInputSelector& InSelector, bool bCaptureMinMax = false);

		template <typename T>
		void Register(FPCGExContext* InContext, const FName InName, EBufferPreloadType InMode = EBufferPreloadType::RawAttribute);

		void Fetch(const TSharedRef<FFacade>& InFacade, const PCGExMT::FScope& Scope);
		void Read(const TSharedRef<FFacade>& InFacade, const int32 ConfigIndex) const;

		///

		using CompletionCallback = std::function<void()>;
		CompletionCallback OnCompleteCallback;

		void StartLoading(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const TSharedPtr<PCGExMT::FAsyncMultiHandle>& InParentHandle = nullptr);

		bool IsLoaded() const { return bLoaded; }

	protected:
		void OnLoadingEnd();
	};

#define PCGEX_TPL(_TYPE, _NAME, ...) \
extern template void FFacadePreloader::Register<_TYPE>(FPCGExContext* InContext, const FPCGAttributePropertyInputSelector& InSelector, bool bCaptureMinMax); \
extern template void FFacadePreloader::Register<_TYPE>(FPCGExContext* InContext, const FName InName, EBufferPreloadType InMode);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

	class PCGEXTENDEDTOOLKIT_API FMultiFacadePreloader : public TSharedFromThis<FMultiFacadePreloader>
	{
		FRWLock LoadingLock;

		TArray<TSharedPtr<FFacadePreloader>> Preloaders;
		TWeakPtr<FPCGContextHandle> WeakHandle;
		bool bLoaded = false;

	public:
		FMultiFacadePreloader(const TArray<TSharedPtr<FFacade>>& InDataFacades);
		FMultiFacadePreloader(const TArray<TSharedRef<FFacade>>& InDataFacades);

		bool IsEmpty() const { return Preloaders.IsEmpty(); }
		int32 Num() const { return Preloaders.Num(); }

		using FPreloaderItCallback = std::function<void(FFacadePreloader&)>;
		void ForEach(FPreloaderItCallback&& It);

		using CompletionCallback = std::function<void()>;
		CompletionCallback OnCompleteCallback;

		bool Validate(FPCGExContext* InContext);
		void StartLoading(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const TSharedPtr<PCGExMT::FAsyncMultiHandle>& InParentHandle = nullptr);

	protected:
		void OnSubloadComplete();
		void OnLoadingEnd();
	};
}
