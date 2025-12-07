// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExTypeErasedProxy.h"

struct FPCGExContext;
class UPCGBasePointData;

namespace PCGExData
{
	class FFacade;
	class IBuffer;
	struct FProxyDescriptor;

	/**
	 * Type-erased proxy helper functions
	 * 
	 * Replaces the template-heavy GetProxyBuffer<T_REAL, T_WORKING> system
	 * with runtime-dispatched functions.
	 * 
	 * Old system: 196+ template instantiations for GetProxyBuffer alone
	 * New system: Single function with internal dispatch
	 */
	namespace ProxyHelpers
	{
		/**
		 * Get a type-erased proxy from a descriptor
		 * This is the main entry point, replacing GetProxyBuffer<T_REAL, T_WORKING>
		 * 
		 * @param InContext - PCG context
		 * @param InDescriptor - Descriptor specifying what to access
		 * @return Type-erased proxy, or nullptr on failure
		 */
		PCGEXTENDEDTOOLKIT_API
		TSharedPtr<FTypeErasedProxy> GetProxy(
			FPCGExContext* InContext,
			const FProxyDescriptor& InDescriptor);

		/**
		 * Get a type-erased proxy from a buffer
		 * 
		 * @param InBuffer - Source buffer
		 * @param WorkingType - Type to work with (conversions handled automatically)
		 * @return Type-erased proxy, or nullptr on failure
		 */
		PCGEXTENDEDTOOLKIT_API
		TSharedPtr<FTypeErasedProxy> GetProxyFromBuffer(
			const TSharedPtr<IBuffer>& InBuffer,
			EPCGMetadataTypes WorkingType);

		/**
		 * Get a constant proxy
		 * Replaces GetConstantProxyBuffer<T>
		 * 
		 * @param ValueType - Type of the constant value
		 * @param Value - Pointer to the constant value
		 * @param WorkingType - Type to work with
		 * @return Type-erased proxy returning constant value
		 */
		PCGEXTENDEDTOOLKIT_API
		TSharedPtr<FTypeErasedProxy> GetConstantProxy(
			EPCGMetadataTypes ValueType,
			const void* Value,
			EPCGMetadataTypes WorkingType);

		/**
		 * Get a constant proxy (typed convenience)
		 */
		template<typename T>
		TSharedPtr<FTypeErasedProxy> GetConstantProxy(const T& Value, EPCGMetadataTypes WorkingType)
		{
			return GetConstantProxy(
				PCGExTypeOps::TTypeToMetadata<T>::Type,
				&Value,
				WorkingType);
		}

		/**
		 * Get a constant proxy with same working type as value type
		 */
		template<typename T>
		TSharedPtr<FTypeErasedProxy> GetConstantProxy(const T& Value)
		{
			constexpr EPCGMetadataTypes Type = PCGExTypeOps::TTypeToMetadata<T>::Type;
			return GetConstantProxy(Type, &Value, Type);
		}

		/**
		 * Get per-field proxies for vector/multi-component types
		 * Replaces GetPerFieldProxyBuffers
		 * 
		 * @param InContext - PCG context
		 * @param InBaseDescriptor - Base descriptor
		 * @param NumDesiredFields - Number of fields to extract
		 * @param OutProxies - Output array of proxies
		 * @return true if successful
		 */
		PCGEXTENDEDTOOLKIT_API
		bool GetPerFieldProxies(
			FPCGExContext* InContext,
			const FProxyDescriptor& InBaseDescriptor,
			int32 NumDesiredFields,
			TArray<TSharedPtr<FTypeErasedProxy>>& OutProxies);

		/**
		 * Try to get a buffer from a descriptor
		 * Non-template version that returns IBuffer
		 * 
		 * @param InContext - PCG context
		 * @param InDescriptor - Descriptor
		 * @param InDataFacade - Data facade
		 * @return Buffer or nullptr
		 */
		PCGEXTENDEDTOOLKIT_API
		TSharedPtr<IBuffer> TryGetBuffer(
			FPCGExContext* InContext,
			const FProxyDescriptor& InDescriptor,
			const TSharedPtr<FFacade>& InDataFacade);
	}

	/**
	 * FTypeErasedBufferHelper - Type-erased version of TBufferHelper
	 * 
	 * Manages a collection of buffers with type-erased access.
	 * Replaces TBufferHelper<Mode> which required template methods.
	 * 
	 * Old system: Template class with template methods, instantiated per-type
	 * New system: Single class with runtime type dispatch
	 */
	class PCGEXTENDEDTOOLKIT_API FTypeErasedBufferHelper : public TSharedFromThis<FTypeErasedBufferHelper>
	{
	public:
		enum class EMode : uint8
		{
			Write = 0,
			Read = 1
		};

	private:
		TSharedPtr<FFacade> DataFacade;
		TMap<FName, TSharedPtr<IBuffer>> BufferMap;
		TMap<FName, TSharedPtr<FTypeErasedProxy>> ProxyMap;
		mutable FRWLock BufferLock;
		EMode Mode;

	public:
		explicit FTypeErasedBufferHelper(const TSharedRef<FFacade>& InDataFacade, EMode InMode = EMode::Write);

		/**
		 * Try to get an existing buffer by name
		 * Returns nullptr if buffer doesn't exist or type mismatch
		 */
		TSharedPtr<IBuffer> TryGetBuffer(FName InName, EPCGMetadataTypes ExpectedType);

		/**
		 * Get or create a buffer by name
		 * For Write mode: creates writable buffer
		 * For Read mode: gets readable buffer (fails if doesn't exist)
		 */
		TSharedPtr<IBuffer> GetBuffer(FName InName, EPCGMetadataTypes Type);

		/**
		 * Get or create a buffer with default value
		 */
		TSharedPtr<IBuffer> GetBuffer(FName InName, EPCGMetadataTypes Type, const void* DefaultValue);

		/**
		 * Get a type-erased proxy for a buffer
		 * Creates the buffer if needed, then wraps in proxy
		 */
		TSharedPtr<FTypeErasedProxy> GetProxy(FName InName, EPCGMetadataTypes RealType, EPCGMetadataTypes WorkingType);

		/**
		 * Set a value by name and index (type-erased)
		 * @param InAttributeName - Attribute name
		 * @param InIndex - Point index
		 * @param Type - Value type
		 * @param Value - Pointer to value
		 * @return true if successful
		 */
		bool SetValue(FName InAttributeName, int32 InIndex, EPCGMetadataTypes Type, const void* Value);

		/**
		 * Get a value by name and index (type-erased)
		 * @param InAttributeName - Attribute name
		 * @param InIndex - Point index
		 * @param Type - Value type
		 * @param OutValue - Pointer to receive value
		 * @return true if successful
		 */
		bool GetValue(FName InAttributeName, int32 InIndex, EPCGMetadataTypes Type, void* OutValue);

		// === Typed convenience methods ===
		// These are thin wrappers that call the type-erased versions

		template<typename T>
		bool SetValue(FName InAttributeName, int32 InIndex, const T& InValue)
		{
			return SetValue(InAttributeName, InIndex, PCGExTypeOps::TTypeToMetadata<T>::Type, &InValue);
		}

		template<typename T>
		bool GetValue(FName InAttributeName, int32 InIndex, T& OutValue)
		{
			return GetValue(InAttributeName, InIndex, PCGExTypeOps::TTypeToMetadata<T>::Type, &OutValue);
		}

		template<typename T>
		T GetValue(FName InAttributeName, int32 InIndex)
		{
			T Result{};
			GetValue(InAttributeName, InIndex, Result);
			return Result;
		}

	private:
		// Internal: Get or create buffer with full type info
		TSharedPtr<IBuffer> GetOrCreateBuffer(FName InName, EPCGMetadataTypes Type, const void* DefaultValue);
	};

	/**
	 * Compatibility aliases
	 * 
	 * These provide backward compatibility with code using the old template system.
	 * They're implemented as inline functions that delegate to the type-erased system.
	 */
	namespace ProxyCompat
	{
		// Replaces: GetProxyBuffer<T_REAL, T_WORKING>(Context, Descriptor, Facade, PointData)
		// Usage: GetProxyBuffer(Context, Descriptor, RealType, WorkingType)
		inline TSharedPtr<FTypeErasedProxy> GetProxyBuffer(
			FPCGExContext* InContext,
			const FProxyDescriptor& InDescriptor,
			EPCGMetadataTypes RealType,
			EPCGMetadataTypes WorkingType)
		{
			// The descriptor should contain RealType/WorkingType, but we accept overrides
			return ProxyHelpers::GetProxy(InContext, InDescriptor);
		}

		// Replaces: GetConstantProxyBuffer<T>(Constant)
		template<typename T>
		TSharedPtr<FTypeErasedProxy> GetConstantProxyBuffer(const T& Constant)
		{
			return ProxyHelpers::GetConstantProxy(Constant);
		}
	}
}