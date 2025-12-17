// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

// Core type operations
#include "Types/PCGExTypeOps.h"

// Type-specific implementations
#include "Types/PCGExTypeOpsNumeric.h"
#include "Types/PCGExTypeOpsVector.h"
#include "Types/PCGExTypeOpsRotation.h"
#include "Types/PCGExTypeOpsString.h"

// Type-erased buffers
//#include "PCGExTypeErasedBuffer.h"

namespace PCGExTypes
{
	//
	// FScopedTypedValue - RAII wrapper for type-erased stack values
	//
	// Provides safe lifecycle management for both POD and complex types (FString, FName, etc.)
	// when stored in stack buffers.
	//
	class PCGEXTENDEDTOOLKIT_API FScopedTypedValue
	{
	public:
		// Calculate maximum size needed across all supported types
		// FTransform is the largest at 80 bytes (FQuat 32 + FVector 24 + FVector 24)
		// Add padding for safety and alignment
		static constexpr int32 BufferSize = 96;
		static constexpr int32 BufferAlignment = 16;

		// Static assertions to ensure buffer is large enough for all types
		static_assert(BufferSize >= sizeof(bool), "Buffer too small for bool");
		static_assert(BufferSize >= sizeof(int32), "Buffer too small for int32");
		static_assert(BufferSize >= sizeof(int64), "Buffer too small for int64");
		static_assert(BufferSize >= sizeof(float), "Buffer too small for float");
		static_assert(BufferSize >= sizeof(double), "Buffer too small for double");
		static_assert(BufferSize >= sizeof(FVector2D), "Buffer too small for FVector2D");
		static_assert(BufferSize >= sizeof(FVector), "Buffer too small for FVector");
		static_assert(BufferSize >= sizeof(FVector4), "Buffer too small for FVector4");
		static_assert(BufferSize >= sizeof(FQuat), "Buffer too small for FQuat");
		static_assert(BufferSize >= sizeof(FRotator), "Buffer too small for FRotator");
		static_assert(BufferSize >= sizeof(FTransform), "Buffer too small for FTransform");
		static_assert(BufferSize >= sizeof(FString), "Buffer too small for FString");
		static_assert(BufferSize >= sizeof(FName), "Buffer too small for FName");
		static_assert(BufferSize >= sizeof(FSoftObjectPath), "Buffer too small for FSoftObjectPath");
		static_assert(BufferSize >= sizeof(FSoftClassPath), "Buffer too small for FSoftClassPath");

	private:
		alignas(BufferAlignment) uint8 Storage[BufferSize];
		EPCGMetadataTypes Type;
		bool bConstructed;

	public:
		// Construct with type - initializes complex types via placement new
		explicit FScopedTypedValue(EPCGMetadataTypes InType);

		// Destructor - calls destructor for complex types
		~FScopedTypedValue();

		// Non-copyable to prevent double-destruction
		FScopedTypedValue(const FScopedTypedValue&) = delete;
		FScopedTypedValue& operator=(const FScopedTypedValue&) = delete;

		// Move constructor
		FScopedTypedValue(FScopedTypedValue&& Other) noexcept;
		FScopedTypedValue& operator=(FScopedTypedValue&&) = delete;

		// Raw access
		FORCEINLINE void* GetRaw() { return Storage; }
		FORCEINLINE const void* GetRaw() const { return Storage; }

		// Typed access
		template <typename T>
		FORCEINLINE T& As() { return *reinterpret_cast<T*>(Storage); }

		template <typename T>
		FORCEINLINE const T& As() const { return *reinterpret_cast<const T*>(Storage); }

		// Type info
		FORCEINLINE EPCGMetadataTypes GetType() const { return Type; }
		FORCEINLINE bool IsConstructed() const { return bConstructed; }

		// Manual lifecycle control (for reuse scenarios)
		void Destruct();
		void Initialize(EPCGMetadataTypes NewType);

		// Type traits helpers
		static bool NeedsLifecycleManagement(EPCGMetadataTypes InType);
		static int32 GetTypeSize(EPCGMetadataTypes InType);
	};


	/**
	 * Convenience functions for common operations
	 */

	// Convert between types (compile-time)
	template <typename TFrom, typename TTo>
	FORCEINLINE TTo Convert(const TFrom& Value)
	{
		return PCGExTypeOps::FTypeOps<TFrom>::template ConvertTo<TTo>(Value);
	}

	// Compute hash for any supported type
	template <typename T>
	FORCEINLINE uint32 ComputeHash(const T& Value)
	{
		return PCGExTypeOps::FTypeOps<T>::Hash(Value);
	}

	// Check if two values are equal
	template <typename T>
	FORCEINLINE bool AreEqual(const T& A, const T& B)
	{
		return A == B;
	}

	// Lerp between values
	template <typename T>
	FORCEINLINE T Lerp(const T& A, const T& B, double Alpha)
	{
		return PCGExTypeOps::FTypeOps<T>::Lerp(A, B, Alpha);
	}

	// Clamp to min/max
	template <typename T>
	FORCEINLINE T Clamp(const T& Value, const T& MinVal, const T& MaxVal)
	{
		T Result = PCGExTypeOps::FTypeOps<T>::Max(Value, MinVal);
		return PCGExTypeOps::FTypeOps<T>::Min(Result, MaxVal);
	}

	template <typename T>
	FORCEINLINE T Abs(const T& A)
	{
		return PCGExTypeOps::FTypeOps<T>::Abs(A);
	}

	template <typename T>
	FORCEINLINE T Factor(const T& A, const double Factor)
	{
		return PCGExTypeOps::FTypeOps<T>::Factor(A, Factor);
	}
}

/**
 * PCGEX_TYPEOPS_DISPATCH - Macro for runtime type dispatch
 * 
 * Generates a switch statement that dispatches to a templated function
 * based on runtime type. Reduces boilerplate in consuming code.
 * 
 * Usage:
 *    PCGEX_TYPEOPS_DISPATCH(RuntimeType, MyFunction, Arg1, Arg2)
 * 
 * Expands to:
 *    switch(RuntimeType) {
 *        case Boolean: return MyFunction<bool>(Arg1, Arg2);
 *        case Integer32: return MyFunction<int32>(Arg1, Arg2);
 *        // ... etc
 *    }
 */
#define PCGEX_TYPEOPS_DISPATCH(TYPE_VAR, FUNC, ...) \
	switch (TYPE_VAR) \
	{ \
	case EPCGMetadataTypes::Boolean: return FUNC<bool>(__VA_ARGS__); \
	case EPCGMetadataTypes::Integer32: return FUNC<int32>(__VA_ARGS__); \
	case EPCGMetadataTypes::Integer64: return FUNC<int64>(__VA_ARGS__); \
	case EPCGMetadataTypes::Float: return FUNC<float>(__VA_ARGS__); \
	case EPCGMetadataTypes::Double: return FUNC<double>(__VA_ARGS__); \
	case EPCGMetadataTypes::Vector2: return FUNC<FVector2D>(__VA_ARGS__); \
	case EPCGMetadataTypes::Vector: return FUNC<FVector>(__VA_ARGS__); \
	case EPCGMetadataTypes::Vector4: return FUNC<FVector4>(__VA_ARGS__); \
	case EPCGMetadataTypes::Quaternion: return FUNC<FQuat>(__VA_ARGS__); \
	case EPCGMetadataTypes::Rotator: return FUNC<FRotator>(__VA_ARGS__); \
	case EPCGMetadataTypes::Transform: return FUNC<FTransform>(__VA_ARGS__); \
	case EPCGMetadataTypes::String: return FUNC<FString>(__VA_ARGS__); \
	case EPCGMetadataTypes::Name: return FUNC<FName>(__VA_ARGS__); \
	case EPCGMetadataTypes::SoftObjectPath: return FUNC<FSoftObjectPath>(__VA_ARGS__); \
	case EPCGMetadataTypes::SoftClassPath: return FUNC<FSoftClassPath>(__VA_ARGS__); \
	default: break; \
	}

/**
 * PCGEX_TYPEOPS_DISPATCH_VOID - Same as above but for void functions
 */
#define PCGEX_TYPEOPS_DISPATCH_VOID(TYPE_VAR, FUNC, ...) \
	switch (TYPE_VAR) \
	{ \
	case EPCGMetadataTypes::Boolean: FUNC<bool>(__VA_ARGS__); break; \
	case EPCGMetadataTypes::Integer32: FUNC<int32>(__VA_ARGS__); break; \
	case EPCGMetadataTypes::Integer64: FUNC<int64>(__VA_ARGS__); break; \
	case EPCGMetadataTypes::Float: FUNC<float>(__VA_ARGS__); break; \
	case EPCGMetadataTypes::Double: FUNC<double>(__VA_ARGS__); break; \
	case EPCGMetadataTypes::Vector2: FUNC<FVector2D>(__VA_ARGS__); break; \
	case EPCGMetadataTypes::Vector: FUNC<FVector>(__VA_ARGS__); break; \
	case EPCGMetadataTypes::Vector4: FUNC<FVector4>(__VA_ARGS__); break; \
	case EPCGMetadataTypes::Quaternion: FUNC<FQuat>(__VA_ARGS__); break; \
	case EPCGMetadataTypes::Rotator: FUNC<FRotator>(__VA_ARGS__); break; \
	case EPCGMetadataTypes::Transform: FUNC<FTransform>(__VA_ARGS__); break; \
	case EPCGMetadataTypes::String: FUNC<FString>(__VA_ARGS__); break; \
	case EPCGMetadataTypes::Name: FUNC<FName>(__VA_ARGS__); break; \
	case EPCGMetadataTypes::SoftObjectPath: FUNC<FSoftObjectPath>(__VA_ARGS__); break; \
	case EPCGMetadataTypes::SoftClassPath: FUNC<FSoftClassPath>(__VA_ARGS__); break; \
	default: break; \
	}
