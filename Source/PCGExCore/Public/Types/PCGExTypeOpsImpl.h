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
#include "Helpers/PCGExMetaHelpers.h"

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
			FConversionTable::Convert(Traits::Type, SrcValue, TargetType, OutValue);
		}

		virtual void ConvertFrom(EPCGMetadataTypes SrcType, const void* SrcValue, void* OutValue) const override
		{
			FConversionTable::Convert(SrcType, SrcValue, Traits::Type, OutValue);
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
#define PCGEX_TPL(_TYPE, _NAME, ...) GetConvertFunction<TFrom, _TYPE>(),
				static const FConvertFn Functions[PCGExTypes::TypesAllocations] = {PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)};
#undef PCGEX_TPL

				check(ToIndex >= 0 && ToIndex < 15)
				return Functions[ToIndex];
			}
		};
	}

	// FTypeOpsRegistry Template Implementations

	template <typename T>
	const ITypeOpsBase* FTypeOpsRegistry::Get() { return &TTypeOpsImpl<T>::GetInstance(); }

	template <typename T>
	EPCGMetadataTypes FTypeOpsRegistry::GetTypeId() { return &TTypeOpsImpl<T>::GetTypeId(); }
}
