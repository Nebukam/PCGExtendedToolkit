// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExBroadcast.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Types/PCGExTypeOps.h"
#include "Types/PCGExTypeOpsImpl.h"
#include "UObject/Object.h"
#include "Utils/PCGValueRange.h"

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

	//
	// FProxyDescriptor - Describes a data source for proxy creation
	//
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
	};

	//
	// IBufferProxy - Type-erased interface for all proxy buffer operations
	//
	// Uses ITypeOpsBase* for runtime type dispatch - eliminates template explosion
	// while providing full conversion/blending capabilities through the type ops system.
	//
	class PCGEXTENDEDTOOLKIT_API IBufferProxy : public TSharedFromThis<IBufferProxy>
	{
	protected:
		// SubSelection support
		bool bWantsSubSelection = false;
		PCGEx::FSubSelection SubSelection;

		// Type operations from registry - provides all conversion & blending
		const PCGExTypeOps::ITypeOpsBase* RealOps = nullptr;
		const PCGExTypeOps::ITypeOpsBase* WorkingOps = nullptr;

	public:
		// Point data reference for property proxies
		UPCGBasePointData* Data = nullptr;

		// Type information
		const EPCGMetadataTypes RealType;
		const EPCGMetadataTypes WorkingType;
		
		// Direct conversion function pointers (from FConversionTable)
		const PCGExTypeOps::FConvertFn WorkingToReal;
		const PCGExTypeOps::FConvertFn RealToWorking;

		explicit IBufferProxy(
			EPCGMetadataTypes InRealType = EPCGMetadataTypes::Unknown,
			EPCGMetadataTypes InWorkingType = EPCGMetadataTypes::Unknown);

		virtual ~IBufferProxy() = default;

		// Validation
		virtual bool Validate(const FProxyDescriptor& InDescriptor) const;

		// Buffer access (for attribute proxies)
		virtual TSharedPtr<IBuffer> GetBuffer() const { return nullptr; }
		virtual bool EnsureReadable() const { return true; }

		// SubSelection configuration
		void SetSubSelection(const PCGEx::FSubSelection& InSubSelection);

		// Role-specific initialization
		virtual void InitForRole(EProxyRole InRole);

		//
		// Type-erased value access - core methods
		//
		virtual void GetVoid(const int32 Index, void* OutValue) const = 0;
		virtual void SetVoid(const int32 Index, const void* Value) const = 0;
		virtual void GetCurrentVoid(const int32 Index, void* OutValue) const { GetVoid(Index, OutValue); }

		// Hash computation
		virtual PCGExValueHash ReadValueHash(const int32 Index) const = 0;

		//
		// Type information accessors
		//
		FORCEINLINE const PCGExTypeOps::ITypeOpsBase* GetRealOps() const { return RealOps; }
		FORCEINLINE const PCGExTypeOps::ITypeOpsBase* GetWorkingOps() const { return WorkingOps; }
		FORCEINLINE bool HasSubSelection() const { return bWantsSubSelection; }
		FORCEINLINE const PCGEx::FSubSelection& GetSubSelection() const { return SubSelection; }

		//
		// Convenience typed accessors - convert through type ops
		//
		template <typename T>
		T Get(const int32 Index) const;

		template <typename T>
		void Set(const int32 Index, const T& Value) const;

		template <typename T>
		T GetCurrent(const int32 Index) const;

		//
		// Converting read methods
		//
#define PCGEX_CONVERTING_READ(_TYPE, _NAME, ...) virtual _TYPE ReadAs##_NAME(const int32 Index) const;
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_CONVERTING_READ)
#undef PCGEX_CONVERTING_READ
	};

	//
	// Template implementations
	//
	template <typename T>
	T IBufferProxy::Get(const int32 Index) const
	{
		alignas(16) uint8 WorkingBuffer[64];
		GetVoid(Index, WorkingBuffer);

		constexpr EPCGMetadataTypes RequestedType = PCGExTypeOps::TTypeToMetadata<T>::Type;
		if (RequestedType == WorkingType)
		{
			return *reinterpret_cast<const T*>(WorkingBuffer);
		}

		T Result{};
		if (WorkingOps)
		{
			WorkingOps->ConvertTo(WorkingBuffer, RequestedType, &Result);
		}
		return Result;
	}

	template <typename T>
	void IBufferProxy::Set(const int32 Index, const T& Value) const
	{
		constexpr EPCGMetadataTypes ValueType = PCGExTypeOps::TTypeToMetadata<T>::Type;
		if (ValueType == WorkingType)
		{
			SetVoid(Index, &Value);
			return;
		}

		alignas(16) uint8 WorkingBuffer[64];
		if (WorkingOps)
		{
			WorkingOps->ConvertFrom(ValueType, &Value, WorkingBuffer);
		}
		SetVoid(Index, WorkingBuffer);
	}

	template <typename T>
	T IBufferProxy::GetCurrent(const int32 Index) const
	{
		alignas(16) uint8 WorkingBuffer[64];
		GetCurrentVoid(Index, WorkingBuffer);

		constexpr EPCGMetadataTypes RequestedType = PCGExTypeOps::TTypeToMetadata<T>::Type;
		if (RequestedType == WorkingType)
		{
			return *reinterpret_cast<const T*>(WorkingBuffer);
		}

		T Result{};
		if (WorkingOps)
		{
			WorkingOps->ConvertTo(WorkingBuffer, RequestedType, &Result);
		}
		return Result;
	}

	//
	// TAttributeBufferProxy - Proxy for attribute buffers
	// Single template parameter (T_REAL) - working type handled via runtime conversion
	//
	template <typename T_REAL>
	class TAttributeBufferProxy : public IBufferProxy
	{
	public:
		TSharedPtr<TBuffer<T_REAL>> Buffer;

		explicit TAttributeBufferProxy(EPCGMetadataTypes InWorkingType);

		virtual void GetVoid(const int32 Index, void* OutValue) const override;
		virtual void SetVoid(const int32 Index, const void* Value) const override;
		virtual void GetCurrentVoid(const int32 Index, void* OutValue) const override;

		virtual TSharedPtr<IBuffer> GetBuffer() const override;
		virtual bool EnsureReadable() const override;

		virtual PCGExValueHash ReadValueHash(const int32 Index) const override;
	};

#pragma region externalization TAttributeBufferProxy

#define PCGEX_TPL(_TYPE, _NAME, ...) extern template class TAttributeBufferProxy<_TYPE>;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	//
	// FPointPropertyProxy - Runtime property dispatch proxy
	//
	class PCGEXTENDEDTOOLKIT_API FPointPropertyProxy : public IBufferProxy
	{
	protected:
		EPCGPointProperties Property = EPCGPointProperties::Density;
		EPCGMetadataTypes PropertyRealType = EPCGMetadataTypes::Unknown;

	public:
		FPointPropertyProxy(EPCGPointProperties InProperty, EPCGMetadataTypes InWorkingType);

		virtual void GetVoid(const int32 Index, void* OutValue) const override;
		virtual void SetVoid(const int32 Index, const void* Value) const override;
		virtual void InitForRole(EProxyRole InRole) override;
		virtual PCGExValueHash ReadValueHash(const int32 Index) const override;

	protected:
		void GetPropertyValue(const int32 Index, void* OutValue) const;
		void SetPropertyValue(const int32 Index, const void* Value) const;
	};

	//
	// FPointExtraPropertyProxy - Extra properties like Index
	//
	class PCGEXTENDEDTOOLKIT_API FPointExtraPropertyProxy : public IBufferProxy
	{
	protected:
		EPCGExtraProperties Property = EPCGExtraProperties::Index;

	public:
		FPointExtraPropertyProxy(EPCGExtraProperties InProperty, EPCGMetadataTypes InWorkingType);

		virtual void GetVoid(const int32 Index, void* OutValue) const override;
		virtual void SetVoid(const int32 Index, const void* Value) const override {}
		virtual PCGExValueHash ReadValueHash(const int32 Index) const override;

		static EPCGMetadataTypes GetPropertyType(EPCGExtraProperties InProperty);
	};

	//
	// TConstantProxy - Constant value proxy
	//
	template <typename T_CONST>
	class TConstantProxy : public IBufferProxy
	{
		T_CONST Constant = T_CONST{};

	public:
		TConstantProxy();

		template <typename T>
		void SetConstant(const T& InValue);

		virtual void GetVoid(const int32 Index, void* OutValue) const override;
		virtual void SetVoid(const int32 Index, const void* Value) const override { check(false); }
		virtual bool Validate(const FProxyDescriptor& InDescriptor) const override;
		virtual PCGExValueHash ReadValueHash(const int32 Index) const override;
	};

#pragma region externalization TConstantProxy

#define PCGEX_TPL(_TYPE, _NAME, ...) extern template class TConstantProxy<_TYPE>;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#define PCGEX_TPL(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...) \
	extern template void TConstantProxy<_TYPE_A>::SetConstant<_TYPE_B>(const _TYPE_B&);
	PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	//
	// TDirectAttributeProxy - Direct attribute access (bypasses buffer)
	//
	template <typename T_REAL>
	class TDirectAttributeProxy : public IBufferProxy
	{
	public:
		const FPCGMetadataAttribute<T_REAL>* InAttribute = nullptr;
		FPCGMetadataAttribute<T_REAL>* OutAttribute = nullptr;

		explicit TDirectAttributeProxy(EPCGMetadataTypes InWorkingType);

		virtual void GetVoid(const int32 Index, void* OutValue) const override;
		virtual void GetCurrentVoid(const int32 Index, void* OutValue) const override;
		virtual void SetVoid(const int32 Index, const void* Value) const override;
		virtual PCGExValueHash ReadValueHash(const int32 Index) const override;
	};

#pragma region externalization TDirectAttributeProxy

#define PCGEX_TPL(_TYPE, _NAME, ...) extern template class TDirectAttributeProxy<_TYPE>;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	//
	// TDirectDataAttributeProxy - Direct data-domain attribute access
	//
	template <typename T_REAL>
	class TDirectDataAttributeProxy : public IBufferProxy
	{
	public:
		const FPCGMetadataAttribute<T_REAL>* InAttribute = nullptr;
		FPCGMetadataAttribute<T_REAL>* OutAttribute = nullptr;

		explicit TDirectDataAttributeProxy(EPCGMetadataTypes InWorkingType);

		virtual void GetVoid(const int32 Index, void* OutValue) const override;
		virtual void GetCurrentVoid(const int32 Index, void* OutValue) const override;
		virtual void SetVoid(const int32 Index, const void* Value) const override;
		virtual PCGExValueHash ReadValueHash(const int32 Index) const override;
	};

#pragma region externalization TDirectDataAttributeProxy

#define PCGEX_TPL(_TYPE, _NAME, ...) extern template class TDirectDataAttributeProxy<_TYPE>;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

} // namespace PCGExData