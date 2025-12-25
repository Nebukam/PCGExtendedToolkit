// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDataCommon.h"
#include "Types/PCGExTypes.h"
#include "Data/PCGExSubSelection.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Types/PCGExTypeOps.h"
#include "UObject/Object.h"
#include "Data/PCGExCachedSubSelection.h"
#include "Helpers/PCGExMetaHelpers.h"

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
	// Type traits for identifying non-trivially-copyable types
	//
	namespace TypeTraits
	{
		// Returns true if the type needs special lifecycle handling
		FORCEINLINE constexpr bool NeedsLifecycleManagement(EPCGMetadataTypes Type)
		{
			switch (Type)
			{
			case EPCGMetadataTypes::String:
			case EPCGMetadataTypes::Name:
			case EPCGMetadataTypes::SoftObjectPath:
			case EPCGMetadataTypes::SoftClassPath: return true;
			default: return false;
			}
		}

		template <typename T>
		constexpr bool TIsComplexType = !std::is_trivially_copyable_v<T>;
	}

	//
	// FProxyDescriptor - Describes a data source for proxy creation
	//
	struct PCGEXCORE_API FProxyDescriptor
	{
		FPCGAttributePropertyInputSelector Selector;
		FSubSelection SubSelection;

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
	class PCGEXCORE_API IBufferProxy : public TSharedFromThis<IBufferProxy>
	{
	protected:
		// SubSelection support
		bool bWantsSubSelection = false;
		FCachedSubSelection CachedSubSelection;

		// Type operations from registry - provides all conversion & blending
		const PCGExTypeOps::ITypeOpsBase* RealOps = nullptr;
		const PCGExTypeOps::ITypeOpsBase* WorkingOps = nullptr;

		// Cached flag for whether working type needs lifecycle management
		bool bWorkingTypeNeedsLifecycle = false;

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
		void SetSubSelection(const FSubSelection& InSubSelection);

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
		FORCEINLINE bool WorkingTypeNeedsLifecycle() const { return bWorkingTypeNeedsLifecycle; }

		//
		// Convenience typed accessors - SAFE versions with proper lifecycle
		//
		template <typename T>
		T Get(const int32 Index) const;

		template <typename T>
		void Set(const int32 Index, const T& Value) const;

		template <typename T>
		T GetCurrent(const int32 Index) const;

		//
		// Converting read methods - SAFE versions
		//
#define PCGEX_CONVERTING_READ(_TYPE, _NAME, ...) virtual _TYPE ReadAs##_NAME(const int32 Index) const;
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_CONVERTING_READ)
#undef PCGEX_CONVERTING_READ

		//
		// Helper to create a scoped value for this proxy's working type
		//
		FORCEINLINE PCGExTypes::FScopedTypedValue CreateScopedWorkingValue() const
		{
			return PCGExTypes::FScopedTypedValue(WorkingType);
		}
	};

	//
	// Template implementations - Now using FScopedTypedValue for safety
	//
	template <typename T>
	T IBufferProxy::Get(const int32 Index) const
	{
		constexpr EPCGMetadataTypes RequestedType = PCGExTypes::TTraits<T>::Type;

		if constexpr (TypeTraits::TIsComplexType<T>)
		{
			// Complex type - use scoped value for proper lifecycle
			PCGExTypes::FScopedTypedValue WorkingValue(WorkingType);
			GetVoid(Index, WorkingValue.GetRaw());

			if (RequestedType == WorkingType) { return WorkingValue.As<T>(); }

			T Result{};
			PCGExTypeOps::FConversionTable::Convert(WorkingType, WorkingValue.GetRaw(), RequestedType, &Result);
			return Result;
		}
		else
		{
			// POD type - can use direct access if types match
			if (RequestedType == WorkingType && !bWorkingTypeNeedsLifecycle)
			{
				// Fast path: direct read for matching POD types
				T Result{};
				GetVoid(Index, &Result);
				return Result;
			}

			// Working type might be complex, use scoped value
			PCGExTypes::FScopedTypedValue WorkingValue(WorkingType);
			GetVoid(Index, WorkingValue.GetRaw());

			if (RequestedType == WorkingType) { return *reinterpret_cast<const T*>(WorkingValue.GetRaw()); }

			T Result{};
			PCGExTypeOps::FConversionTable::Convert(WorkingType, WorkingValue.GetRaw(), RequestedType, &Result);
			return Result;
		}
	}

	template <typename T>
	void IBufferProxy::Set(const int32 Index, const T& Value) const
	{
		constexpr EPCGMetadataTypes ValueType = PCGExTypes::TTraits<T>::Type;

		if (ValueType == WorkingType)
		{
			// Types match - direct set
			SetVoid(Index, &Value);
			return;
		}

		// Need conversion - use scoped value for working type
		PCGExTypes::FScopedTypedValue WorkingValue(WorkingType);
		PCGExTypeOps::FConversionTable::Convert(ValueType, &Value, WorkingType, WorkingValue.GetRaw());
		SetVoid(Index, WorkingValue.GetRaw());
	}

	template <typename T>
	T IBufferProxy::GetCurrent(const int32 Index) const
	{
		constexpr EPCGMetadataTypes RequestedType = PCGExTypes::TTraits<T>::Type;

		// Always use scoped value for safety
		PCGExTypes::FScopedTypedValue WorkingValue(WorkingType);
		GetCurrentVoid(Index, WorkingValue.GetRaw());

		if (RequestedType == WorkingType)
		{
			if constexpr (TypeTraits::TIsComplexType<T>) { return WorkingValue.As<T>(); }
			else { return *reinterpret_cast<const T*>(WorkingValue.GetRaw()); }
		}

		T Result{};
		PCGExTypeOps::FConversionTable::Convert(WorkingType, WorkingValue.GetRaw(), RequestedType, &Result);
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
	class PCGEXCORE_API FPointPropertyProxy : public IBufferProxy
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
	class PCGEXCORE_API FPointExtraPropertyProxy : public IBufferProxy
	{
	protected:
		EPCGExtraProperties Property = EPCGExtraProperties::Index;

	public:
		FPointExtraPropertyProxy(EPCGExtraProperties InProperty, EPCGMetadataTypes InWorkingType);

		virtual void GetVoid(const int32 Index, void* OutValue) const override;

		virtual void SetVoid(const int32 Index, const void* Value) const override
		{
		}

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
		explicit TConstantProxy(EPCGMetadataTypes InWorkingType);

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
}
