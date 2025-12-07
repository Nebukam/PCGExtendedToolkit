// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "TypeOperations/PCGExTypeOpsImpl.h"

class FPCGMetadataAttributeBase;
struct FPCGExContext;
class UPCGBasePointData;

namespace PCGExData
{
	class IBuffer;
	class FFacade;
	struct FProxyDescriptor;

	template <typename T>
	class TBuffer;

	/**
	 * EProxySourceType - Identifies the backing data source for a proxy
	 */
	enum class EProxySourceType : uint8
	{
		None,
		AttributeBuffer,    // TBuffer<T> from FFacade
		PointProperty,      // EPCGPointProperties (Position, Rotation, Scale, etc.)
		PointExtraProperty, // EPCGExtraProperties (Index, etc.)
		Constant,           // Constant value
		DirectAttribute,    // Direct FPCGMetadataAttribute access (slow)
		DirectDataAttribute // Direct data attribute access (slow)
	};

	/**
	 * FTypeErasedProxy - Type-erased buffer/property proxy
	 * 
	 * Replaces the template-heavy proxy hierarchy with a single class that uses
	 * function pointers for type dispatch. Supports all data sources:
	 * - Attribute buffers (TBuffer<T>)
	 * - Point properties (EPCGPointProperties)
	 * - Extra properties (EPCGExtraProperties)
	 * - Constant values
	 * - Direct attribute access
	 * 
	 * Old system: 196+ class instantiations per proxy type
	 * New system: 1 class + function pointer dispatch
	 */
	class PCGEXTENDEDTOOLKIT_API FTypeErasedProxy : public TSharedFromThis<FTypeErasedProxy>
	{
	public:
		// Function pointer types for type-erased operations
		// Get from source (input side)
		using FGetFn = void(*)(const FTypeErasedProxy* Self, int32 Index, void* OutValue);
		// Set to output
		using FSetFn = void(*)(const FTypeErasedProxy* Self, int32 Index, const void* Value);
		// Get current output value
		using FGetCurrentFn = void(*)(const FTypeErasedProxy* Self, int32 Index, void* OutValue);
		// Get value hash
		using FGetHashFn = PCGExValueHash(*)(const FTypeErasedProxy* Self, int32 Index);
		
	protected:
		// Source type
		EProxySourceType SourceType = EProxySourceType::None;
		
		// Type information
		EPCGMetadataTypes RealType = EPCGMetadataTypes::Unknown;
		EPCGMetadataTypes WorkingType = EPCGMetadataTypes::Unknown;
		
		// Type operations
		const PCGExTypeOps::ITypeOpsBase* RealTypeOps = nullptr;
		const PCGExTypeOps::ITypeOpsBase* WorkingTypeOps = nullptr;
		
		// Conversion functions
		PCGExTypeOps::FConvertFn ToWorkingConverter = nullptr;
		PCGExTypeOps::FConvertFn FromWorkingConverter = nullptr;
		
		// Function pointers for operations (set by derived/factory)
		FGetFn GetFn = nullptr;
		FSetFn SetFn = nullptr;
		FGetCurrentFn GetCurrentFn = nullptr;
		FGetHashFn GetHashFn = nullptr;
		
		// SubSelection for vector component access
		bool bWantsSubSelection = false;
		int32 SubSelectionIndex = -1;  // -1 = no sub-selection, 0-3 = component index
		
		// Data sources (only one is active based on SourceType)
		union
		{
			TSharedPtr<IBuffer>* BufferPtr;           // For AttributeBuffer
			UPCGBasePointData* PointData;              // For PointProperty/ExtraProperty
			void* ConstantValuePtr;                    // For Constant (heap allocated)
			FPCGMetadataAttributeBase* DirectAttribute; // For DirectAttribute
		};
		
		// Point property enum (when SourceType == PointProperty or PointExtraProperty)
		int32 PropertyEnum = 0;
		
		// Storage for buffer shared ptr (since union can't hold non-trivial types)
		TSharedPtr<IBuffer> BufferStorage;
		
		// Constant value storage (max size = FTransform)
		alignas(16) uint8 ConstantStorage[sizeof(FTransform)];
		
	public:
		FTypeErasedProxy();
		virtual ~FTypeErasedProxy();
		
		// Disable copy (due to union)
		FTypeErasedProxy(const FTypeErasedProxy&) = delete;
		FTypeErasedProxy& operator=(const FTypeErasedProxy&) = delete;
		
		// Move is allowed
		FTypeErasedProxy(FTypeErasedProxy&& Other) noexcept;
		FTypeErasedProxy& operator=(FTypeErasedProxy&& Other) noexcept;
		
		// === Core Access Methods (Working Type) ===
		
		/**
		 * Get value at index, converted to working type
		 * Reads from input/source
		 */
		void Get(int32 Index, void* OutValue) const;
				
		/**
		 * Set value at index, converted from working type
		 * Writes to output
		 */
		void Set(int32 Index, const void* Value) const;
				
		/**
		 * Get current output value at index
		 * Reads from output (what was written)
		 */
		void GetCurrent(int32 Index, void* OutValue) const;
		
		
		/**
		 * Get value hash at index
		 */
		uint64 GetValueHash(int32 Index) const;
				
		// === Typed Access (Convenience) ===
		
		template<typename T>
		T GetTpl(int32 Index) const
		{
			T Result{};
			const EPCGMetadataTypes RequestedType = PCGExTypeOps::TTypeToMetadata<T>::Type;
			
			if (RequestedType == WorkingType)
			{
				Get(Index, &Result);
			}
			else
			{
				alignas(16) uint8 WorkingValue[sizeof(FTransform)];
				Get(Index, WorkingValue);
				PCGExTypeOps::FConversionTable::Convert(WorkingType, WorkingValue, RequestedType, &Result);
			}
			return Result;
		}
		
		template<typename T>
		void SetTpl(int32 Index, const T& Value) const
		{
			const EPCGMetadataTypes RequestedType = PCGExTypeOps::TTypeToMetadata<T>::Type;
			
			if (RequestedType == WorkingType)
			{
				Set(Index, &Value);
			}
			else
			{
				alignas(16) uint8 WorkingValue[sizeof(FTransform)];
				PCGExTypeOps::FConversionTable::Convert(RequestedType, &Value, WorkingType, WorkingValue);
				Set(Index, WorkingValue);
			}
		}
		
		template<typename T>
		T GetCurrentTpl(int32 Index) const
		{
			T Result{};
			const EPCGMetadataTypes RequestedType = PCGExTypeOps::TTypeToMetadata<T>::Type;
			
			if (RequestedType == WorkingType)
			{
				GetCurrent(Index, &Result);
			}
			else
			{
				alignas(16) uint8 WorkingValue[sizeof(FTransform)];
				GetCurrent(Index, WorkingValue);
				PCGExTypeOps::FConversionTable::Convert(WorkingType, WorkingValue, RequestedType, &Result);
			}
			return Result;
		}
		
		// === ReadAs* Methods (Compatibility) ===
		
		bool ReadAsBoolean(int32 Index) const { return GetTpl<bool>(Index); }
		int32 ReadAsInteger32(int32 Index) const { return GetTpl<int32>(Index); }
		int64 ReadAsInteger64(int32 Index) const { return GetTpl<int64>(Index); }
		float ReadAsFloat(int32 Index) const { return GetTpl<float>(Index); }
		double ReadAsDouble(int32 Index) const { return GetTpl<double>(Index); }
		FVector2D ReadAsVector2D(int32 Index) const { return GetTpl<FVector2D>(Index); }
		FVector ReadAsVector(int32 Index) const { return GetTpl<FVector>(Index); }
		FVector4 ReadAsVector4(int32 Index) const { return GetTpl<FVector4>(Index); }
		FQuat ReadAsQuat(int32 Index) const { return GetTpl<FQuat>(Index); }
		FRotator ReadAsRotator(int32 Index) const { return GetTpl<FRotator>(Index); }
		FTransform ReadAsTransform(int32 Index) const { return GetTpl<FTransform>(Index); }
		FString ReadAsString(int32 Index) const { return GetTpl<FString>(Index); }
		FName ReadAsName(int32 Index) const { return GetTpl<FName>(Index); }
		FSoftObjectPath ReadAsSoftObjectPath(int32 Index) const { return GetTpl<FSoftObjectPath>(Index); }
		FSoftClassPath ReadAsSoftClassPath(int32 Index) const { return GetTpl<FSoftClassPath>(Index); }
		
		// === Properties ===
		
		bool IsValid() const { return SourceType != EProxySourceType::None && GetFn != nullptr; }
		EProxySourceType GetSourceType() const { return SourceType; }
		EPCGMetadataTypes GetRealType() const { return RealType; }
		EPCGMetadataTypes GetWorkingType() const { return WorkingType; }
		bool RequiresConversion() const { return RealType != WorkingType; }
		bool HasSubSelection() const { return bWantsSubSelection; }
		int32 GetSubSelectionIndex() const { return SubSelectionIndex; }
		
		const PCGExTypeOps::ITypeOpsBase* GetRealTypeOps() const { return RealTypeOps; }
		const PCGExTypeOps::ITypeOpsBase* GetWorkingTypeOps() const { return WorkingTypeOps; }
		
		TSharedPtr<IBuffer> GetBuffer() const;
		UPCGBasePointData* GetPointData() const;
		
		// === SubSelection ===
		
		void SetSubSelection(int32 ComponentIndex);
		void ClearSubSelection();
		
		// === Validation ===
		
		bool Validate(const FProxyDescriptor& InDescriptor) const;
		
		// === Internal Accessors (for accessor functions) ===
		
		const uint8* GetConstantStorage() const { return ConstantStorage; }
		uint8* GetMutableConstantStorage() { return ConstantStorage; }
		
	protected:
		// Initialize type operations and converters
		void InitializeTypeOps();
		
		// Friends for factory access
		friend class FTypeErasedProxyFactory;
	};

	/**
	 * FTypeErasedProxyFactory - Creates type-erased proxies from descriptors
	 * 
	 * Handles the runtime dispatch to create the appropriate proxy type.
	 * This is the main entry point for creating proxies.
	 */
	class PCGEXTENDEDTOOLKIT_API FTypeErasedProxyFactory
	{
	public:
		// Create proxy from descriptor (main entry point)
		static TSharedPtr<FTypeErasedProxy> Create(
			FPCGExContext* InContext,
			const FProxyDescriptor& InDescriptor);
		
		// Create proxy from attribute buffer
		static TSharedPtr<FTypeErasedProxy> CreateFromBuffer(
			const TSharedPtr<IBuffer>& InBuffer,
			EPCGMetadataTypes InWorkingType);
		
		// Create constant proxy
		template<typename T>
		static TSharedPtr<FTypeErasedProxy> CreateConstant(const T& Value, EPCGMetadataTypes InWorkingType);
		
		// Create constant proxy (runtime type)
		static TSharedPtr<FTypeErasedProxy> CreateConstant(
			EPCGMetadataTypes ValueType,
			const void* Value,
			EPCGMetadataTypes InWorkingType);
		
		// Create proxy for point property
		static TSharedPtr<FTypeErasedProxy> CreateForPointProperty(
			UPCGBasePointData* InPointData,
			int32 PropertyEnum,  // EPCGPointProperties or EPCGExtraProperties
			bool bIsExtraProperty,
			EPCGMetadataTypes InWorkingType);
		
		// Create proxy for direct attribute access
		static TSharedPtr<FTypeErasedProxy> CreateDirectAttribute(
			FPCGMetadataAttributeBase* InAttribute,
			FPCGMetadataAttributeBase* OutAttribute,
			UPCGBasePointData* InPointData,
			EPCGMetadataTypes InWorkingType);
	};

	// === Template Implementation for CreateConstant ===
	
	template<typename T>
	TSharedPtr<FTypeErasedProxy> FTypeErasedProxyFactory::CreateConstant(const T& Value, EPCGMetadataTypes InWorkingType)
	{
		return CreateConstant(
			PCGExTypeOps::TTypeToMetadata<T>::Type,
			&Value,
			InWorkingType);
	}
}