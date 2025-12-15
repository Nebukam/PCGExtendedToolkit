// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "Metadata/PCGMetadataAttributeTraits.h"
#include "PCGExCommon.h"
#include "Details/PCGExDetailsAxis.h"

/**
 * PCGEx Type Operations System
 * 
 * This system centralizes all type-specific operations (conversion, blending, hashing)
 * into per-type semantic objects. Instead of 14×14×22 template instantiations,
 * we have 14 type operation objects + runtime dispatch.
 * 
 * Architecture:
 * - FTypeOps<T>: Static per-type operations (conversion, blend functions, hash)
 * - ITypeOpsBase: Type-erased interface for runtime dispatch
 * - FTypeOpsRegistry: Global registry mapping EPCGMetadataTypes to ops
 */

namespace PCGExTypeOps
{
	/**
	 * Single field selection identifiers
	 */
	enum class ESingleField : uint8
	{
		X             = 0,
		Y             = 1,
		Z             = 2,
		W             = 3,
		Length        = 4,
		SquaredLength = 5,
		Volume        = 6,
		Sum           = 7,
	};

	/**
	 * Transform component parts
	 */
	enum class ETransformPart : uint8
	{
		Position = 0,
		Rotation = 1,
		Scale    = 2,
	};

	// Forward declarations
	class ITypeOpsBase;

	template <typename T>
	struct FTypeOps;

	// Type Traits - Compile time type classification

	template <typename T>
	struct TTypeTraits
	{
		static constexpr EPCGMetadataTypes Type = EPCGMetadataTypes::Unknown;
		static constexpr int16 TypeId = static_cast<int16>(Type);
		
		static constexpr bool bIsNumeric = false;
		static constexpr bool bIsVector = false;
		static constexpr bool bIsRotation = false;
		static constexpr bool bIsString = false;
		static constexpr bool bSupportsLerp = false;
		static constexpr bool bSupportsMinMax = false;
		static constexpr bool bSupportsArithmetic = false;
	};

	// Numeric types
	template <>
	struct TTypeTraits<bool>
	{
		static constexpr EPCGMetadataTypes Type = EPCGMetadataTypes::Boolean;
		static constexpr int16 TypeId = static_cast<int16>(Type);
		
		static constexpr bool bIsNumeric = true;
		static constexpr bool bIsVector = false;
		static constexpr bool bIsRotation = false;
		static constexpr bool bIsString = false;
		static constexpr bool bSupportsLerp = false;
		static constexpr bool bSupportsMinMax = true;
		static constexpr bool bSupportsArithmetic = false;
	};

	template <>
	struct TTypeTraits<int32>
	{
		static constexpr EPCGMetadataTypes Type = EPCGMetadataTypes::Integer32;
		static constexpr int16 TypeId = static_cast<int16>(Type);
		
		static constexpr bool bIsNumeric = true;
		static constexpr bool bIsVector = false;
		static constexpr bool bIsRotation = false;
		static constexpr bool bIsString = false;
		static constexpr bool bSupportsLerp = true;
		static constexpr bool bSupportsMinMax = true;
		static constexpr bool bSupportsArithmetic = true;
	};

	template <>
	struct TTypeTraits<int64>
	{
		static constexpr EPCGMetadataTypes Type = EPCGMetadataTypes::Integer64;
		static constexpr int16 TypeId = static_cast<int16>(Type);
		
		static constexpr bool bIsNumeric = true;
		static constexpr bool bIsVector = false;
		static constexpr bool bIsRotation = false;
		static constexpr bool bIsString = false;
		static constexpr bool bSupportsLerp = true;
		static constexpr bool bSupportsMinMax = true;
		static constexpr bool bSupportsArithmetic = true;
	};

	template <>
	struct TTypeTraits<float>
	{
		static constexpr EPCGMetadataTypes Type = EPCGMetadataTypes::Float;
		static constexpr int16 TypeId = static_cast<int16>(Type);
		
		static constexpr bool bIsNumeric = true;
		static constexpr bool bIsVector = false;
		static constexpr bool bIsRotation = false;
		static constexpr bool bIsString = false;
		static constexpr bool bSupportsLerp = true;
		static constexpr bool bSupportsMinMax = true;
		static constexpr bool bSupportsArithmetic = true;
	};

	template <>
	struct TTypeTraits<double>
	{
		static constexpr EPCGMetadataTypes Type = EPCGMetadataTypes::Double;
		static constexpr int16 TypeId = static_cast<int16>(Type);
		
		static constexpr bool bIsNumeric = true;
		static constexpr bool bIsVector = false;
		static constexpr bool bIsRotation = false;
		static constexpr bool bIsString = false;
		static constexpr bool bSupportsLerp = true;
		static constexpr bool bSupportsMinMax = true;
		static constexpr bool bSupportsArithmetic = true;
	};

	// Vector types
	template <>
	struct TTypeTraits<FVector2D>
	{
		static constexpr EPCGMetadataTypes Type = EPCGMetadataTypes::Vector2;
		static constexpr int16 TypeId = static_cast<int16>(Type);
		
		static constexpr bool bIsNumeric = false;
		static constexpr bool bIsVector = true;
		static constexpr bool bIsRotation = false;
		static constexpr bool bIsString = false;
		static constexpr bool bSupportsLerp = true;
		static constexpr bool bSupportsMinMax = true;
		static constexpr bool bSupportsArithmetic = true;
	};

	template <>
	struct TTypeTraits<FVector>
	{
		static constexpr EPCGMetadataTypes Type = EPCGMetadataTypes::Vector;
		static constexpr int16 TypeId = static_cast<int16>(Type);
		
		static constexpr bool bIsNumeric = false;
		static constexpr bool bIsVector = true;
		static constexpr bool bIsRotation = false;
		static constexpr bool bIsString = false;
		static constexpr bool bSupportsLerp = true;
		static constexpr bool bSupportsMinMax = true;
		static constexpr bool bSupportsArithmetic = true;
	};

	template <>
	struct TTypeTraits<FVector4>
	{
		static constexpr EPCGMetadataTypes Type = EPCGMetadataTypes::Vector4;
		static constexpr int16 TypeId = static_cast<int16>(Type);
		
		static constexpr bool bIsNumeric = false;
		static constexpr bool bIsVector = true;
		static constexpr bool bIsRotation = false;
		static constexpr bool bIsString = false;
		static constexpr bool bSupportsLerp = true;
		static constexpr bool bSupportsMinMax = true;
		static constexpr bool bSupportsArithmetic = true;
	};

	// Rotation types
	template <>
	struct TTypeTraits<FQuat>
	{
		static constexpr EPCGMetadataTypes Type = EPCGMetadataTypes::Quaternion;
		static constexpr int16 TypeId = static_cast<int16>(Type);
		
		static constexpr bool bIsNumeric = false;
		static constexpr bool bIsVector = false;
		static constexpr bool bIsRotation = true;
		static constexpr bool bIsString = false;
		static constexpr bool bSupportsLerp = true;
		static constexpr bool bSupportsMinMax = false;
		static constexpr bool bSupportsArithmetic = false;
	};

	template <>
	struct TTypeTraits<FRotator>
	{
		static constexpr EPCGMetadataTypes Type = EPCGMetadataTypes::Rotator;
		static constexpr int16 TypeId = static_cast<int16>(Type);
		
		static constexpr bool bIsNumeric = false;
		static constexpr bool bIsVector = false;
		static constexpr bool bIsRotation = true;
		static constexpr bool bIsString = false;
		static constexpr bool bSupportsLerp = true;
		static constexpr bool bSupportsMinMax = true;
		static constexpr bool bSupportsArithmetic = true;
	};

	template <>
	struct TTypeTraits<FTransform>
	{
		static constexpr EPCGMetadataTypes Type = EPCGMetadataTypes::Transform;
		static constexpr int16 TypeId = static_cast<int16>(Type);
		
		static constexpr bool bIsNumeric = false;
		static constexpr bool bIsVector = false;
		static constexpr bool bIsRotation = false;
		static constexpr bool bIsString = false;
		static constexpr bool bSupportsLerp = true;
		static constexpr bool bSupportsMinMax = false;
		static constexpr bool bSupportsArithmetic = false;
	};

	// String types
	template <>
	struct TTypeTraits<FString>
	{
		static constexpr EPCGMetadataTypes Type = EPCGMetadataTypes::String;
		static constexpr int16 TypeId = static_cast<int16>(Type);
		
		static constexpr bool bIsNumeric = false;
		static constexpr bool bIsVector = false;
		static constexpr bool bIsRotation = false;
		static constexpr bool bIsString = true;
		static constexpr bool bSupportsLerp = false;
		static constexpr bool bSupportsMinMax = false;
		static constexpr bool bSupportsArithmetic = false;
	};

	template <>
	struct TTypeTraits<FName>
	{
		static constexpr EPCGMetadataTypes Type = EPCGMetadataTypes::Name;
		static constexpr int16 TypeId = static_cast<int16>(Type);
		
		static constexpr bool bIsNumeric = false;
		static constexpr bool bIsVector = false;
		static constexpr bool bIsRotation = false;
		static constexpr bool bIsString = true;
		static constexpr bool bSupportsLerp = false;
		static constexpr bool bSupportsMinMax = false;
		static constexpr bool bSupportsArithmetic = false;
	};

	template <>
	struct TTypeTraits<FSoftObjectPath>
	{
		static constexpr EPCGMetadataTypes Type = EPCGMetadataTypes::SoftObjectPath;
		static constexpr int16 TypeId = static_cast<int16>(Type);
		
		static constexpr bool bIsNumeric = false;
		static constexpr bool bIsVector = false;
		static constexpr bool bIsRotation = false;
		static constexpr bool bIsString = true;
		static constexpr bool bSupportsLerp = false;
		static constexpr bool bSupportsMinMax = false;
		static constexpr bool bSupportsArithmetic = false;
	};

	template <>
	struct TTypeTraits<FSoftClassPath>
	{
		static constexpr EPCGMetadataTypes Type = EPCGMetadataTypes::SoftClassPath;
		static constexpr int16 TypeId = static_cast<int16>(Type);
		
		static constexpr bool bIsNumeric = false;
		static constexpr bool bIsVector = false;
		static constexpr bool bIsRotation = false;
		static constexpr bool bIsString = true;
		static constexpr bool bSupportsLerp = false;
		static constexpr bool bSupportsMinMax = false;
		static constexpr bool bSupportsArithmetic = false;
	};

	// Type-Erased Operation Interface

	/**
	 * Type-erased interface for all type operations.
	 * Allows runtime dispatch without templates at call sites.
	 */
	class PCGEXTENDEDTOOLKIT_API ITypeOpsBase
	{
	public:
		virtual ~ITypeOpsBase() = default;

		// Type information
		virtual EPCGMetadataTypes GetTypeId() const = 0;
		virtual FString GetTypeName() const = 0;
		virtual int32 GetTypeSize() const = 0;
		virtual int32 GetTypeAlignment() const = 0;
		virtual bool SupportsLerp() const = 0;
		virtual bool SupportsMinMax() const = 0;
		virtual bool SupportsArithmetic() const = 0;

		// Default value operations
		virtual void SetDefault(void* OutValue) const = 0;
		virtual void Copy(const void* Src, void* Dst) const = 0;

		// Hash operations
		virtual PCGExValueHash ComputeHash(const void* Value) const = 0;

		// Conversion - implemented by derived to avoid virtual call overhead
		// Convert from this type to target type
		virtual void ConvertTo(const void* SrcValue, EPCGMetadataTypes TargetType, void* OutValue) const = 0;
		// Convert from source type to this type
		virtual void ConvertFrom(EPCGMetadataTypes SrcType, const void* SrcValue, void* OutValue) const = 0;

		// Blend operations - type-erased versions
		// All blend functions: void Blend(const void* A, const void* B, void* Out, double Weight)
		virtual void BlendAdd(const void* A, const void* B, void* Out) const = 0;
		virtual void BlendSub(const void* A, const void* B, void* Out) const = 0;
		virtual void BlendMult(const void* A, const void* B, void* Out) const = 0;
		virtual void BlendDiv(const void* A, double Divisor, void* Out) const = 0;
		virtual void BlendLerp(const void* A, const void* B, double Weight, void* Out) const = 0;
		virtual void BlendMin(const void* A, const void* B, void* Out) const = 0;
		virtual void BlendMax(const void* A, const void* B, void* Out) const = 0;
		virtual void BlendAverage(const void* A, const void* B, void* Out) const = 0;
		virtual void BlendWeightedAdd(const void* A, const void* B, double Weight, void* Out) const = 0;
		virtual void BlendWeightedSub(const void* A, const void* B, double Weight, void* Out) const = 0;
		virtual void BlendCopyA(const void* A, const void* B, void* Out) const = 0;
		virtual void BlendCopyB(const void* A, const void* B, void* Out) const = 0;
		virtual void BlendUnsignedMin(const void* A, const void* B, void* Out) const = 0;
		virtual void BlendUnsignedMax(const void* A, const void* B, void* Out) const = 0;
		virtual void BlendAbsoluteMin(const void* A, const void* B, void* Out) const = 0;
		virtual void BlendAbsoluteMax(const void* A, const void* B, void* Out) const = 0;
		virtual void BlendHash(const void* A, const void* B, void* Out) const = 0;
		virtual void BlendUnsignedHash(const void* A, const void* B, void* Out) const = 0;
		virtual void BlendModSimple(const void* A, double Modulo, void* Out) const = 0;
		virtual void BlendModComplex(const void* A, const void* B, void* Out) const = 0;


		// Weight/Average accumulation helpers
		virtual void BlendWeight(const void* A, const void* B, double Weight, void* Out) const = 0;
		virtual void NormalizeWeight(const void* A, double TotalWeight, void* Out) const = 0;

	};

	// Type Operations Registry

	/**
	 * Global registry for type operations.
	 * Provides O(1) lookup of type ops by EPCGMetadataTypes enum.
	 */
	class PCGEXTENDEDTOOLKIT_API FTypeOpsRegistry
	{
	public:
		static constexpr int32 NumSupportedTypes = 14;

		// Get ops for a specific type
		static const ITypeOpsBase* Get(EPCGMetadataTypes Type);

		// Get ops with template type
		template <typename T>
		static const ITypeOpsBase* Get();

		// Type ID conversion
		template <typename T>
		static EPCGMetadataTypes GetTypeId();

		static EPCGMetadataTypes GetTypeIdFromIndex(int32 Index);
		static int32 GetIndexFromTypeId(EPCGMetadataTypes Type);

		// Initialize the registry (called automatically)
		static void Initialize();

	private:
		static TArray<TUniquePtr<ITypeOpsBase>> TypeOps;
		static bool bInitialized;
	};

	// Conversion Function Pointer Types

	// Function pointer type for conversion: void Convert(const void* Src, void* Dst)
	using FConvertFn = void(*)(const void* Src, void* Dst);

	/**
	 * Conversion dispatch table.
	 * 14x14 table of function pointers for all type pair conversions.
	 */
	class PCGEXTENDEDTOOLKIT_API FConversionTable
	{
	public:
		// Convert between any two supported types
		static void Convert(
			EPCGMetadataTypes FromType, const void* FromValue,
			EPCGMetadataTypes ToType, void* ToValue);

		// Get the conversion function pointer for a specific pair
		static FConvertFn GetConversionFn(EPCGMetadataTypes FromType, EPCGMetadataTypes ToType);

		// Initialize the table (called automatically)
		static void Initialize();

	private:
		static FConvertFn Table[14][14];
		static bool bInitialized;
	};

	// Blend Function Pointer Types

	// Standard blend: C = Blend(A, B)
	using FBlendBinaryFn = void(*)(const void* A, const void* B, void* Out);
	// Weighted blend: C = Blend(A, B, Weight)
	using FBlendWeightedFn = void(*)(const void* A, const void* B, double Weight, void* Out);
	// Scalar blend: C = Blend(A, Scalar)
	using FBlendScalarFn = void(*)(const void* A, double Scalar, void* Out);

	// Type-Safe Wrapper Templates

	/**
	 * Type-safe conversion wrapper that uses the type-erased system.
	 */
	template <typename TFrom, typename TTo>
	FORCEINLINE TTo Convert(const TFrom& Value)
	{
		TTo Result{};
		FConversionTable::Convert(TTypeTraits<TFrom>::Type, &Value, TTypeTraits<TTo>::Type, &Result);
		return Result;
	}

	/**
	 * Type-safe hash wrapper.
	 */
	template <typename T>
	FORCEINLINE PCGExValueHash ComputeHash(const T& Value)
	{
		const ITypeOpsBase* Ops = FTypeOpsRegistry::Get<T>();
		return Ops->ComputeHash(&Value);
	}
}
