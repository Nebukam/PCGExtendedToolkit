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

// Type-erased wrappers and registry
#include "Types/PCGExTypeOpsImpl.h"

// Blend operations
#include "Data/BlendOperations/PCGExBlendOperations.h"

// Type-erased buffers
//#include "PCGExTypeErasedBuffer.h"

namespace PCGExTypes
{
	/**
	 * Convenience functions for common operations
	 */
	
	// Convert between types (compile-time)
	template<typename TFrom, typename TTo>
	FORCEINLINE TTo Convert(const TFrom& Value)
	{
		return PCGExTypeOps::FTypeOps<TFrom>::template ConvertTo<TTo>(Value);
	}
	
	// Compute hash for any supported type
	template<typename T>
	FORCEINLINE uint32 ComputeHash(const T& Value)
	{
		return PCGExTypeOps::FTypeOps<T>::Hash(Value);
	}
	
	// Check if two values are equal
	template<typename T>
	FORCEINLINE bool AreEqual(const T& A, const T& B)
	{
		return A == B;
	}
	
	// Lerp between values
	template<typename T>
	FORCEINLINE T Lerp(const T& A, const T& B, double Alpha)
	{
		return PCGExTypeOps::FTypeOps<T>::Lerp(A, B, Alpha);
	}
	
	// Clamp to min/max
	template<typename T>
	FORCEINLINE T Clamp(const T& Value, const T& MinVal, const T& MaxVal)
	{
		T Result = PCGExTypeOps::FTypeOps<T>::Max(Value, MinVal);
		return PCGExTypeOps::FTypeOps<T>::Min(Result, MaxVal);
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