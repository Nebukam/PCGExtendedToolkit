// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExBroadcast.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "UObject/Object.h"

struct FPCGExContext;
class UPCGBasePointData;

template <typename T>
class FPCGMetadataAttribute;

namespace PCGExData
{
	class IBuffer;

	template <typename T>
	class TBuffer;

	enum class EProxyRole : uint8
	{
		Read,
		Write
	};

	struct PCGEXTENDEDTOOLKIT_API FProxyDescriptor
	{
		FPCGAttributePropertyInputSelector Selector;
		PCGEx::FSubSelection SubSelection;

		EIOSide Side = EIOSide::In;
		EProxyRole Role = EProxyRole::Read;

		EPCGMetadataTypes RealType = EPCGMetadataTypes::Unknown;
		EPCGMetadataTypes WorkingType = EPCGMetadataTypes::Unknown;

		TWeakPtr<FFacade> DataFacade;

		const UPCGBasePointData* PointData = nullptr;

		bool bIsConstant = false;
		bool bWantsDirect = false;

		FProxyDescriptor() = default;

		explicit FProxyDescriptor(const TSharedPtr<FFacade>& InDataFacade, const EProxyRole InRole = EProxyRole::Read)
			: Role(InRole), DataFacade(InDataFacade)
		{
		}

		~FProxyDescriptor() = default;
		void UpdateSubSelection();
		bool SetFieldIndex(const int32 InFieldIndex);

		bool Capture(FPCGExContext* InContext, const FString& Path, const EIOSide InSide = EIOSide::Out, const bool bRequired = true);
		bool Capture(FPCGExContext* InContext, const FPCGAttributePropertyInputSelector& InSelector, const EIOSide InSide = EIOSide::Out, const bool bRequired = true);

		bool CaptureStrict(FPCGExContext* InContext, const FString& Path, const EIOSide InSide = EIOSide::Out, const bool bRequired = true);
		bool CaptureStrict(FPCGExContext* InContext, const FPCGAttributePropertyInputSelector& InSelector, const EIOSide InSide = EIOSide::Out, const bool bRequired = true);

		//static FProxyDescriptor CreateForPointProperty(UPCGBasePointData* PointData);
	};

	class IBufferProxy : public TSharedFromThis<IBufferProxy>
	{
	public:
		UPCGBasePointData* Data = nullptr;
		PCGEx::FSubSelection SubSelection;
		EPCGMetadataTypes RealType = EPCGMetadataTypes::Unknown;
		EPCGMetadataTypes WorkingType = EPCGMetadataTypes::Unknown;

#if WITH_EDITOR
		// This is purely for debugging purposes
		FProxyDescriptor Descriptor;
#endif

		IBufferProxy() = default;
		virtual ~IBufferProxy() = default;

		virtual bool Validate(const FProxyDescriptor& InDescriptor) const { return InDescriptor.RealType == RealType && InDescriptor.WorkingType == WorkingType; }
		virtual TSharedPtr<IBuffer> GetBuffer() const { return nullptr; }
		virtual bool EnsureReadable() const { return true; }

#define PCGEX_CONVERTING_READ(_TYPE, _NAME, ...) virtual _TYPE ReadAs##_NAME(const int32 Index) const;
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_CONVERTING_READ)
#undef PCGEX_CONVERTING_READ
	};

	template <typename T_WORKING>
	class TBufferProxy : public IBufferProxy
	{
	public:
		TBufferProxy();

		virtual T_WORKING Get(const int32 Index) const = 0;
		virtual void Set(const int32 Index, const T_WORKING& Value) const = 0;
		virtual T_WORKING GetCurrent(const int32 Index) const { return Get(Index); };
		virtual TSharedPtr<IBuffer> GetBuffer() const override { return nullptr; }

#define PCGEX_CONVERTING_READ(_TYPE, _NAME, ...) virtual _TYPE ReadAs##_NAME(const int32 Index) const override;
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_CONVERTING_READ)
#undef PCGEX_CONVERTING_READ
	};

	template <typename T_REAL, typename T_WORKING, bool bSubSelection>
	class TAttributeBufferProxy : public TBufferProxy<T_WORKING>
	{
		using TBufferProxy<T_WORKING>::SubSelection;

	public:
		TSharedPtr<TBuffer<T_REAL>> Buffer;

		TAttributeBufferProxy();

		virtual T_WORKING Get(const int32 Index) const override;
		virtual void Set(const int32 Index, const T_WORKING& Value) const override;
		virtual T_WORKING GetCurrent(const int32 Index) const override;

		virtual TSharedPtr<IBuffer> GetBuffer() const override;
		virtual bool EnsureReadable() const override;
	};

	template <typename T_REAL, typename T_WORKING, bool bSubSelection, EPCGPointProperties PROPERTY>
	class TPointPropertyProxy : public TBufferProxy<T_WORKING>
	{
		using TBufferProxy<T_WORKING>::SubSelection;
		using TBufferProxy<T_WORKING>::Data;

	public:
		TPointPropertyProxy();

		virtual T_WORKING Get(const int32 Index) const override;
		virtual void Set(const int32 Index, const T_WORKING& Value) const override;
	};

#pragma region externalization TPointPropertyProxy

#define PCGEX_TPL(_TYPE, _NAME, _REALTYPE, _PROPERTY)\
extern template class TPointPropertyProxy<_REALTYPE, _TYPE, true, _PROPERTY>;\
extern template class TPointPropertyProxy<_REALTYPE, _TYPE, false, _PROPERTY>;

#define PCGEX_TPL_LOOP(_PROPERTY, _NAME, _TYPE, _NATIVETYPE)	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL, _TYPE, _PROPERTY)

	PCGEX_FOREACH_POINTPROPERTY(PCGEX_TPL_LOOP)

#undef PCGEX_TPL_LOOP
#undef PCGEX_TPL

#pragma endregion

	template <typename T_REAL, typename T_WORKING, bool bSubSelection, EPCGExtraProperties PROPERTY>
	class TPointExtraPropertyProxy : public TBufferProxy<T_WORKING>
	{
		using TBufferProxy<T_WORKING>::SubSelection;

	public:
		TSharedPtr<TBuffer<T_REAL>> Buffer;

		TPointExtraPropertyProxy();

		virtual T_WORKING Get(const int32 Index) const override;

		virtual void Set(const int32 Index, const T_WORKING& Value) const override
		{
			// Well, no
		}
	};

#pragma region externalization TPointExtraPropertyProxy

#define PCGEX_TPL(_TYPE, _NAME, _REALTYPE, _PROPERTY)\
extern template class TPointExtraPropertyProxy<_REALTYPE, _TYPE, true, _PROPERTY>;\
extern template class TPointExtraPropertyProxy<_REALTYPE, _TYPE, false, _PROPERTY>;

#define PCGEX_TPL_LOOP(_PROPERTY, _NAME, _TYPE)	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL, _TYPE, _PROPERTY)

	PCGEX_FOREACH_EXTRAPROPERTY(PCGEX_TPL_LOOP)

#undef PCGEX_TPL_LOOP
#undef PCGEX_TPL

#pragma endregion

	template <typename T_WORKING>
	class TConstantProxy : public TBufferProxy<T_WORKING>
	{
		T_WORKING Constant = T_WORKING{};

	public:
		TConstantProxy();

		template <typename T>
		void SetConstant(const T& InValue);

		virtual T_WORKING Get(const int32 Index) const override { return Constant; }

		virtual void Set(const int32 Index, const T_WORKING& Value) const override
		{
			// This should never happen, check the callstack
			check(false)
		}

		virtual bool Validate(const FProxyDescriptor& InDescriptor) const override { return InDescriptor.WorkingType == this->WorkingType; }
	};

	template <typename T_REAL, typename T_WORKING, bool bSubSelection>
	class TDirectAttributeProxy : public TBufferProxy<T_WORKING>
	{
		// A memory-friendly but super slow proxy version that works with Setter/Getter on the attribute
		// TODO : Implement support for this to replace old "soft" metadata ops

		using TBufferProxy<T_WORKING>::SubSelection;
		using TBufferProxy<T_WORKING>::Data;

	public:
		FPCGMetadataAttribute<T_REAL>* InAttribute = nullptr;
		FPCGMetadataAttribute<T_REAL>* OutAttribute = nullptr;

		TDirectAttributeProxy();

		virtual T_WORKING Get(const int32 Index) const override;
		virtual T_WORKING GetCurrent(const int32 Index) const override;
		virtual void Set(const int32 Index, const T_WORKING& Value) const override;
	};

	template <typename T_REAL, typename T_WORKING, bool bSubSelection>
	class TDirectDataAttributeProxy : public TBufferProxy<T_WORKING>
	{
		// A memory-friendly but super slow proxy version that works with Setter/Getter on the attribute
		// TODO : Implement support for this to replace old "soft" metadata ops

		using TBufferProxy<T_WORKING>::SubSelection;
		using TBufferProxy<T_WORKING>::Data;

	public:
		FPCGMetadataAttribute<T_REAL>* InAttribute = nullptr;
		FPCGMetadataAttribute<T_REAL>* OutAttribute = nullptr;

		TDirectDataAttributeProxy();

		virtual T_WORKING Get(const int32 Index) const override;
		virtual T_WORKING GetCurrent(const int32 Index) const override;
		virtual void Set(const int32 Index, const T_WORKING& Value) const override;
	};

#pragma region externalization

#define PCGEX_TPL(_TYPE, _NAME, ...) \
extern template class TBufferProxy<_TYPE>;\
PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#define PCGEX_TPL(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...) \
	extern template class TAttributeBufferProxy<_TYPE_A, _TYPE_B, true>;\
	extern template class TAttributeBufferProxy<_TYPE_A, _TYPE_B, false>;\
	extern template class TDirectAttributeProxy<_TYPE_A, _TYPE_B, true>;\
	extern template class TDirectAttributeProxy<_TYPE_A, _TYPE_B, false>;\
	extern template class TDirectDataAttributeProxy<_TYPE_A, _TYPE_B, true>;\
	extern template class TDirectDataAttributeProxy<_TYPE_A, _TYPE_B, false>;
	PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	template <typename T_REAL>
	TSharedPtr<TBuffer<T_REAL>> TryGetBuffer(FPCGExContext* InContext, const FProxyDescriptor& InDescriptor, const TSharedPtr<FFacade>& InDataFacade);

	template <typename T_REAL>
	void TryGetInOutAttr(const FProxyDescriptor& InDescriptor, const TSharedPtr<FFacade>& InDataFacade, const FPCGMetadataAttribute<T_REAL>*& OutInAttribute, FPCGMetadataAttribute<T_REAL>*& OutOutAttribute);

#pragma region externalization

#define PCGEX_TPL(_TYPE, _NAME, ...) \
extern template void TryGetInOutAttr<_TYPE>(const FProxyDescriptor& InDescriptor, const TSharedPtr<FFacade>& InDataFacade, const FPCGMetadataAttribute<_TYPE>*& OutInAttribute, FPCGMetadataAttribute<_TYPE>*& OutOutAttribute); \
extern template TSharedPtr<TBuffer<_TYPE>> TryGetBuffer<_TYPE>(FPCGExContext* InContext, const FProxyDescriptor& InDescriptor, const TSharedPtr<FFacade>& InDataFacade);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	template <typename T_REAL, typename T_WORKING>
	TSharedPtr<IBufferProxy> GetProxyBuffer(FPCGExContext* InContext, const FProxyDescriptor& InDescriptor, const TSharedPtr<FFacade>& InDataFacade, UPCGBasePointData* PointData);

#pragma region externalization

#define PCGEX_TPL(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...) \
extern template TSharedPtr<IBufferProxy> GetProxyBuffer<_TYPE_A, _TYPE_B>(FPCGExContext* InContext, const FProxyDescriptor& InDescriptor, const TSharedPtr<FFacade>& InDataFacade, UPCGBasePointData* PointData);
	PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	TSharedPtr<IBufferProxy> GetProxyBuffer(FPCGExContext* InContext, const FProxyDescriptor& InDescriptor);

	template <typename T>
	TSharedPtr<IBufferProxy> GetConstantProxyBuffer(const T& Constant);

#pragma region externalization

#define PCGEX_TPL(_TYPE, _NAME, ...) \
extern template TSharedPtr<IBufferProxy> GetConstantProxyBuffer<_TYPE>(const _TYPE& Constant);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	bool GetPerFieldProxyBuffers(
		FPCGExContext* InContext,
		const FProxyDescriptor& InBaseDescriptor,
		const int32 NumDesiredFields,
		TArray<TSharedPtr<IBufferProxy>>& OutProxies);
}
