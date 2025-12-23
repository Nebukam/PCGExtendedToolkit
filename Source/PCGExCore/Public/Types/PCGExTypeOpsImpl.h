// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExTypeOps.h"
#include "PCGExTypeOpsNumeric.h"
#include "PCGExTypeOpsVector.h"
#include "PCGExTypeOpsRotation.h"
#include "PCGExTypeOpsString.h"
#include "PCGExTypeTraits.h"

namespace PCGExTypeOps
{
	// TTypeOpsImpl - Concrete implementation of ITypeOpsBase for each type

	/**
	 * TTypeOpsImpl<T> - Type-erased implementation wrapper
	 * 
	 * This class bridges the static FTypeOps<T> templates to the runtime
	 * ITypeOpsBase interface, enabling virtual dispatch without template
	 * instantiation at call sites.
	 * 
	 */
	template <typename T>
	class PCGEXCORE_API TTypeOpsImpl : public ITypeOpsBase
	{
	public:
		using TypeOps = FTypeOps<T>;
		using Traits = PCGExTypes::TTraits<T>;

		//~ Begin ITypeOpsBase interface

		// Type Information

		virtual EPCGMetadataTypes GetTypeId() const override { return Traits::Type; }

		virtual FString GetTypeName() const override
		{
			// TODO!
			return FString();
		}

		virtual int32 GetTypeSize() const override { return sizeof(T); }
		virtual int32 GetTypeAlignment() const override { return alignof(T); }
		virtual bool SupportsLerp() const override { return Traits::bSupportsLerp; }
		virtual bool SupportsMinMax() const override { return Traits::bSupportsMinMax; }
		virtual bool SupportsArithmetic() const override { return Traits::bSupportsArithmetic; }

		// Default Value Operations

		virtual void SetDefault(void* OutValue) const override { new(OutValue) T(); }
		virtual void Copy(const void* Src, void* Dst) const override { *static_cast<T*>(Dst) = *static_cast<const T*>(Src); }

		// Hash Operations

		virtual PCGExValueHash ComputeHash(const void* Value) const override { return TypeOps::Hash(*static_cast<const T*>(Value)); }

		// Conversion Operations

		virtual void ConvertTo(const void* SrcValue, EPCGMetadataTypes TargetType, void* OutValue) const override
		{
			const T& Src = *static_cast<const T*>(SrcValue);

			switch (TargetType)
			{
			case EPCGMetadataTypes::Boolean: *static_cast<bool*>(OutValue) = TypeOps::template ConvertTo<bool>(Src);
				break;
			case EPCGMetadataTypes::Integer32: *static_cast<int32*>(OutValue) = TypeOps::template ConvertTo<int32>(Src);
				break;
			case EPCGMetadataTypes::Integer64: *static_cast<int64*>(OutValue) = TypeOps::template ConvertTo<int64>(Src);
				break;
			case EPCGMetadataTypes::Float: *static_cast<float*>(OutValue) = TypeOps::template ConvertTo<float>(Src);
				break;
			case EPCGMetadataTypes::Double: *static_cast<double*>(OutValue) = TypeOps::template ConvertTo<double>(Src);
				break;
			case EPCGMetadataTypes::Vector2: *static_cast<FVector2D*>(OutValue) = TypeOps::template ConvertTo<FVector2D>(Src);
				break;
			case EPCGMetadataTypes::Vector: *static_cast<FVector*>(OutValue) = TypeOps::template ConvertTo<FVector>(Src);
				break;
			case EPCGMetadataTypes::Vector4: *static_cast<FVector4*>(OutValue) = TypeOps::template ConvertTo<FVector4>(Src);
				break;
			case EPCGMetadataTypes::Quaternion: *static_cast<FQuat*>(OutValue) = TypeOps::template ConvertTo<FQuat>(Src);
				break;
			case EPCGMetadataTypes::Rotator: *static_cast<FRotator*>(OutValue) = TypeOps::template ConvertTo<FRotator>(Src);
				break;
			case EPCGMetadataTypes::Transform: *static_cast<FTransform*>(OutValue) = TypeOps::template ConvertTo<FTransform>(Src);
				break;
			case EPCGMetadataTypes::String: *static_cast<FString*>(OutValue) = TypeOps::template ConvertTo<FString>(Src);
				break;
			case EPCGMetadataTypes::Name: *static_cast<FName*>(OutValue) = TypeOps::template ConvertTo<FName>(Src);
				break;
			case EPCGMetadataTypes::SoftObjectPath: *static_cast<FSoftObjectPath*>(OutValue) = TypeOps::template ConvertTo<FSoftObjectPath>(Src);
				break;
			case EPCGMetadataTypes::SoftClassPath: *static_cast<FSoftClassPath*>(OutValue) = TypeOps::template ConvertTo<FSoftClassPath>(Src);
				break;
			default: SetDefault(OutValue);
				break;
			}
		}

		virtual void ConvertFrom(EPCGMetadataTypes SrcType, const void* SrcValue, void* OutValue) const override
		{
			T& Dst = *static_cast<T*>(OutValue);

			switch (SrcType)
			{
			case EPCGMetadataTypes::Boolean: Dst = TypeOps::template ConvertFrom<bool>(*static_cast<const bool*>(SrcValue));
				break;
			case EPCGMetadataTypes::Integer32: Dst = TypeOps::template ConvertFrom<int32>(*static_cast<const int32*>(SrcValue));
				break;
			case EPCGMetadataTypes::Integer64: Dst = TypeOps::template ConvertFrom<int64>(*static_cast<const int64*>(SrcValue));
				break;
			case EPCGMetadataTypes::Float: Dst = TypeOps::template ConvertFrom<float>(*static_cast<const float*>(SrcValue));
				break;
			case EPCGMetadataTypes::Double: Dst = TypeOps::template ConvertFrom<double>(*static_cast<const double*>(SrcValue));
				break;
			case EPCGMetadataTypes::Vector2: Dst = TypeOps::template ConvertFrom<FVector2D>(*static_cast<const FVector2D*>(SrcValue));
				break;
			case EPCGMetadataTypes::Vector: Dst = TypeOps::template ConvertFrom<FVector>(*static_cast<const FVector*>(SrcValue));
				break;
			case EPCGMetadataTypes::Vector4: Dst = TypeOps::template ConvertFrom<FVector4>(*static_cast<const FVector4*>(SrcValue));
				break;
			case EPCGMetadataTypes::Quaternion: Dst = TypeOps::template ConvertFrom<FQuat>(*static_cast<const FQuat*>(SrcValue));
				break;
			case EPCGMetadataTypes::Rotator: Dst = TypeOps::template ConvertFrom<FRotator>(*static_cast<const FRotator*>(SrcValue));
				break;
			case EPCGMetadataTypes::Transform: Dst = TypeOps::template ConvertFrom<FTransform>(*static_cast<const FTransform*>(SrcValue));
				break;
			case EPCGMetadataTypes::String: Dst = TypeOps::template ConvertFrom<FString>(*static_cast<const FString*>(SrcValue));
				break;
			case EPCGMetadataTypes::Name: Dst = TypeOps::template ConvertFrom<FName>(*static_cast<const FName*>(SrcValue));
				break;
			case EPCGMetadataTypes::SoftObjectPath: Dst = TypeOps::template ConvertFrom<FSoftObjectPath>(*static_cast<const FSoftObjectPath*>(SrcValue));
				break;
			case EPCGMetadataTypes::SoftClassPath: Dst = TypeOps::template ConvertFrom<FSoftClassPath>(*static_cast<const FSoftClassPath*>(SrcValue));
				break;
			default: Dst = T();
				break;
			}
		}

		//Blend Operations

		virtual void BlendAdd(const void* A, const void* B, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::Add(*static_cast<const T*>(A), *static_cast<const T*>(B));
		}

		virtual void BlendSub(const void* A, const void* B, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::Sub(*static_cast<const T*>(A), *static_cast<const T*>(B));
		}

		virtual void BlendMult(const void* A, const void* B, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::Mult(*static_cast<const T*>(A), *static_cast<const T*>(B));
		}

		virtual void BlendDiv(const void* A, double Divisor, void* Out) const override
		{
			// Div by scalar - use Weight with 1/Divisor for types that support it
			if (Divisor != 0.0) { *static_cast<T*>(Out) = TypeOps::Div(*static_cast<const T*>(A), Divisor); }
			else { *static_cast<T*>(Out) = *static_cast<const T*>(A); }
		}

		virtual void BlendLerp(const void* A, const void* B, double Weight, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::Lerp(*static_cast<const T*>(A), *static_cast<const T*>(B), Weight);
		}

		virtual void BlendMin(const void* A, const void* B, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::Min(*static_cast<const T*>(A), *static_cast<const T*>(B));
		}

		virtual void BlendMax(const void* A, const void* B, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::Max(*static_cast<const T*>(A), *static_cast<const T*>(B));
		}

		virtual void BlendAverage(const void* A, const void* B, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::Average(*static_cast<const T*>(A), *static_cast<const T*>(B));
		}

		virtual void BlendWeightedAdd(const void* A, const void* B, double Weight, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::WeightedAdd(*static_cast<const T*>(A), *static_cast<const T*>(B), Weight);
		}

		virtual void BlendWeightedSub(const void* A, const void* B, double Weight, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::WeightedSub(*static_cast<const T*>(A), *static_cast<const T*>(B), Weight);
		}

		virtual void BlendCopyA(const void* A, const void* B, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::CopyA(*static_cast<const T*>(A), *static_cast<const T*>(B));
		}

		virtual void BlendCopyB(const void* A, const void* B, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::CopyB(*static_cast<const T*>(A), *static_cast<const T*>(B));
		}

		virtual void BlendUnsignedMin(const void* A, const void* B, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::UnsignedMin(*static_cast<const T*>(A), *static_cast<const T*>(B));
		}

		virtual void BlendUnsignedMax(const void* A, const void* B, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::UnsignedMax(*static_cast<const T*>(A), *static_cast<const T*>(B));
		}

		virtual void BlendAbsoluteMin(const void* A, const void* B, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::AbsoluteMin(*static_cast<const T*>(A), *static_cast<const T*>(B));
		}

		virtual void BlendAbsoluteMax(const void* A, const void* B, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::AbsoluteMax(*static_cast<const T*>(A), *static_cast<const T*>(B));
		}

		virtual void BlendHash(const void* A, const void* B, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::NaiveHash(*static_cast<const T*>(A), *static_cast<const T*>(B));
		}

		virtual void BlendUnsignedHash(const void* A, const void* B, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::UnsignedHash(*static_cast<const T*>(A), *static_cast<const T*>(B));
		}

		virtual void BlendModSimple(const void* A, double Modulo, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::ModSimple(*static_cast<const T*>(A), Modulo);
		}

		virtual void BlendModComplex(const void* A, const void* B, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::ModComplex(*static_cast<const T*>(A), *static_cast<const T*>(B));
		}

		virtual void BlendWeight(const void* A, const void* B, double Weight, void* Out) const override
		{
			// Weight accumulation: Out = A + (B * Weight)
			*static_cast<T*>(Out) = TypeOps::WeightedAdd(*static_cast<const T*>(A), *static_cast<const T*>(B), Weight);
		}

		virtual void NormalizeWeight(const void* A, double TotalWeight, void* Out) const override
		{
			// Weight accumulation: Out = A * (1 / TotalWeight)
			*static_cast<T*>(Out) = TypeOps::NormalizeWeight(*static_cast<const T*>(A), TotalWeight);
		}

		virtual void Abs(const void* A, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::Abs(*static_cast<const T*>(A));
		}

		virtual void Factor(const void* A, const double Factor, void* Out) const override
		{
			*static_cast<T*>(Out) = TypeOps::Factor(*static_cast<const T*>(A), Factor);
		}

		//~ End ITypeOpsBase interface

		//Static instance accessor

		static TTypeOpsImpl<T>& GetInstance()
		{
			static TTypeOpsImpl<T> Instance;
			return Instance;
		}
	};

	// Conversion Function Generation

	namespace ConversionFunctions
	{
		/**
		 * Generate a conversion function from TFrom to TTo
		 */
		template <typename TFrom, typename TTo>
		void ConvertImpl(const void* From, void* To) { *static_cast<TTo*>(To) = FTypeOps<TFrom>::template ConvertTo<TTo>(*static_cast<const TFrom*>(From)); }

		/**
		 * Identity conversion (same type)
		 */
		template <typename T>
		void ConvertIdentity(const void* From, void* To) { *static_cast<T*>(To) = *static_cast<const T*>(From); }

		/**
		 * Get conversion function for a type pair
		 */
		template <typename TFrom, typename TTo>
		constexpr FConvertFn GetConvertFunction()
		{
			if constexpr (std::is_same_v<TFrom, TTo>) { return &ConvertIdentity<TFrom>; }
			else { return &ConvertImpl<TFrom, TTo>; }
		}

		/**
		 * Row of conversion functions from one source type to all target types
		 */
		template <typename TFrom>
		struct TConversionRow
		{
			static FConvertFn GetFunction(int32 ToIndex)
			{
				static const FConvertFn Functions[15] = {
					GetConvertFunction<TFrom, bool>(),            // 0: Boolean
					GetConvertFunction<TFrom, int32>(),           // 1: Integer32
					GetConvertFunction<TFrom, int64>(),           // 2: Integer64
					GetConvertFunction<TFrom, float>(),           // 3: Float
					GetConvertFunction<TFrom, double>(),          // 4: Double
					GetConvertFunction<TFrom, FVector2D>(),       // 5: Vector2
					GetConvertFunction<TFrom, FVector>(),         // 6: Vector
					GetConvertFunction<TFrom, FVector4>(),        // 7: Vector4
					GetConvertFunction<TFrom, FQuat>(),           // 8: Quaternion
					GetConvertFunction<TFrom, FRotator>(),        // 9: Rotator
					GetConvertFunction<TFrom, FTransform>(),      // 10: Transform
					GetConvertFunction<TFrom, FString>(),         // 11: String
					GetConvertFunction<TFrom, FName>(),           // 12: Name
					GetConvertFunction<TFrom, FSoftObjectPath>(), // 13: SoftObjectPath
					GetConvertFunction<TFrom, FSoftClassPath>()   // 14: SoftClassPath
				};

				if (ToIndex >= 0 && ToIndex < 15)
				{
					return Functions[ToIndex];
				}
				return nullptr;
			}
		};
	}

	// FTypeOpsRegistry Implementation Helpers

	// Type index mapping
	inline int32 GetTypeIndex(EPCGMetadataTypes Type)
	{
		switch (Type)
		{
		case EPCGMetadataTypes::Boolean: return 0;
		case EPCGMetadataTypes::Integer32: return 1;
		case EPCGMetadataTypes::Integer64: return 2;
		case EPCGMetadataTypes::Float: return 3;
		case EPCGMetadataTypes::Double: return 4;
		case EPCGMetadataTypes::Vector2: return 5;
		case EPCGMetadataTypes::Vector: return 6;
		case EPCGMetadataTypes::Vector4: return 7;
		case EPCGMetadataTypes::Quaternion: return 8;
		case EPCGMetadataTypes::Rotator: return 9;
		case EPCGMetadataTypes::Transform: return 10;
		case EPCGMetadataTypes::String: return 11;
		case EPCGMetadataTypes::Name: return 12;
		case EPCGMetadataTypes::SoftObjectPath: return 13;
		case EPCGMetadataTypes::SoftClassPath: return 14;
		default: return -1;
		}
	}

	inline EPCGMetadataTypes GetTypeFromIndex(int32 Index)
	{
		static constexpr EPCGMetadataTypes Types[] = {
			EPCGMetadataTypes::Boolean,
			EPCGMetadataTypes::Integer32,
			EPCGMetadataTypes::Integer64,
			EPCGMetadataTypes::Float,
			EPCGMetadataTypes::Double,
			EPCGMetadataTypes::Vector2,
			EPCGMetadataTypes::Vector,
			EPCGMetadataTypes::Vector4,
			EPCGMetadataTypes::Quaternion,
			EPCGMetadataTypes::Rotator,
			EPCGMetadataTypes::Transform,
			EPCGMetadataTypes::String,
			EPCGMetadataTypes::Name,
			EPCGMetadataTypes::SoftObjectPath,
			EPCGMetadataTypes::SoftClassPath
		};

		if (Index >= 0 && Index < 15)
		{
			return Types[Index];
		}
		return EPCGMetadataTypes::Unknown;
	}

	// FTypeOpsRegistry Template Implementations

	template <typename T>
	const ITypeOpsBase* FTypeOpsRegistry::Get() { return &TTypeOpsImpl<T>::GetInstance(); }

	template <typename T>
	EPCGMetadataTypes FTypeOpsRegistry::GetTypeId() { return &TTypeOpsImpl<T>::GetTypeId(); }
}
