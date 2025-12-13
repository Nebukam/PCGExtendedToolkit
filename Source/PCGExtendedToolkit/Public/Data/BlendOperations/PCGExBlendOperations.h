// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Types/PCGExTypeOpsImpl.h"

namespace PCGExDataBlending
{
	/**
	 * IBlendOperation - Type-erased interface for blend operations
	 * 
	 * Provides a runtime-polymorphic interface for blending values of any type.
	 * Eliminates the need for template instantiation per blend mode.
	 * 
	 * Replaces: TProxyDataBlender<T, MODE, bool> (616 instantiations)
	 * With: TBlendOperationImpl<T> (14 instantiations) + runtime mode dispatch
	 */
	class PCGEXTENDEDTOOLKIT_API IBlendOperation
	{
	public:
		virtual ~IBlendOperation() = default;

		// Core blend: Out = Blend(A, B, Weight)
		virtual void Blend(const void* A, const void* B, double Weight, void* Out) const = 0;

		// Multi-blend operations for accumulation patterns
		virtual void BeginMulti(void* Accumulator) const = 0;
		virtual void Accumulate(const void* Source, void* Accumulator, double Weight) const = 0;
		virtual void EndMulti(void* Accumulator, double TotalWeight, int32 Count) const = 0;

		// Properties
		virtual EPCGMetadataTypes GetWorkingType() const = 0;
		virtual EPCGExABBlendingType GetBlendMode() const = 0;
		virtual bool RequiresReset() const = 0;

		// Stack buffer helpers
		virtual int32 GetValueSize() const = 0;
		virtual int32 GetValueAlignment() const = 0;

		// Default value initialization
		virtual void InitDefault(void* Value) const = 0;
	};

	/**
	 * TBlendOperationImpl<T> - Templated implementation of IBlendOperation
	 * 
	 * Only 14 instantiations (one per type) instead of 616.
	 * Blend mode selection is done via function pointer at construction time.
	 */
	template <typename T>
	class TBlendOperationImpl final : public IBlendOperation
	{
	public:
		using BlendFn = T(*)(const T&, const T&, double);
		using AccumulateFn = void(*)(const T&, T&, double);
		using FinalizeFn = void(*)(T&, double, int32);

	private:
		BlendFn BlendFunc = nullptr;
		AccumulateFn AccumulateFunc = nullptr;
		FinalizeFn FinalizeFunc = nullptr;
		EPCGExABBlendingType Mode;
		bool bResetForMulti;
		T DefaultValue;

	public:
		TBlendOperationImpl(
			EPCGExABBlendingType InMode,
			bool bInResetForMulti,
			BlendFn InBlendFunc,
			AccumulateFn InAccumulateFunc = nullptr,
			FinalizeFn InFinalizeFunc = nullptr)
			: BlendFunc(InBlendFunc)
			  , AccumulateFunc(InAccumulateFunc)
			  , FinalizeFunc(InFinalizeFunc)
			  , Mode(InMode)
			  , bResetForMulti(bInResetForMulti)
			  , DefaultValue()
		{
			// Set default accumulate/finalize if not provided
			if (!AccumulateFunc)
			{
				AccumulateFunc = &DefaultAccumulate;
			}
			if (!FinalizeFunc)
			{
				FinalizeFunc = &DefaultFinalize;
			}
		}

		//~ Begin IBlendOperation interface

		virtual void Blend(const void* A, const void* B, double Weight, void* Out) const override
		{
			*static_cast<T*>(Out) = BlendFunc(
				*static_cast<const T*>(A),
				*static_cast<const T*>(B),
				Weight);
		}

		virtual void BeginMulti(void* Accumulator) const override
		{
			if (bResetForMulti)
			{
				new(Accumulator) T(DefaultValue);
			}
		}

		virtual void Accumulate(const void* Source, void* Accumulator, double Weight) const override
		{
			AccumulateFunc(
				*static_cast<const T*>(Source),
				*static_cast<T*>(Accumulator),
				Weight);
		}

		virtual void EndMulti(void* Accumulator, double TotalWeight, int32 Count) const override
		{
			FinalizeFunc(*static_cast<T*>(Accumulator), TotalWeight, Count);
		}

		virtual EPCGMetadataTypes GetWorkingType() const override
		{
			return PCGExTypeOps::TTypeToMetadata<T>::Type;
		}

		virtual EPCGExABBlendingType GetBlendMode() const override
		{
			return Mode;
		}

		virtual bool RequiresReset() const override
		{
			return bResetForMulti;
		}

		virtual int32 GetValueSize() const override
		{
			return sizeof(T);
		}

		virtual int32 GetValueAlignment() const override
		{
			return alignof(T);
		}

		virtual void InitDefault(void* Value) const override
		{
			new(Value) T(DefaultValue);
		}

		//~ End IBlendOperation interface

	private:
		// Default implementations
		static void DefaultAccumulate(const T& Source, T& Accumulator, double Weight)
		{
			Accumulator = PCGExTypeOps::FTypeOps<T>::WeightedAdd(Accumulator, Source, Weight);
		}

		static void DefaultFinalize(T& Accumulator, double TotalWeight, int32 Count)
		{
			if (TotalWeight > 0.0)
			{
				Accumulator = PCGExTypeOps::FTypeOps<T>::Div(Accumulator, 1.0 / TotalWeight);
			}
		}
	};

	/**
	 * FBlendFunctions - Static blend function implementations per type
	 * 
	 * Centralizes all blend mode implementations. Each function is shared
	 * across all uses of that blend mode for a given type.
	 */
	namespace BlendFunctions
	{
		// === Generic implementations using FTypeOps ===

		template <typename T>
		T Add(const T& A, const T& B, double /*Weight*/)
		{
			return PCGExTypeOps::FTypeOps<T>::Add(A, B);
		}

		template <typename T>
		T Sub(const T& A, const T& B, double /*Weight*/)
		{
			return PCGExTypeOps::FTypeOps<T>::Sub(A, B);
		}

		template <typename T>
		T Mult(const T& A, const T& B, double /*Weight*/)
		{
			return PCGExTypeOps::FTypeOps<T>::Mult(A, B);
		}

		template <typename T>
		T Div(const T& A, const T& /*B*/, double Weight)
		{
			return PCGExTypeOps::FTypeOps<T>::Div(A, Weight);
		}

		template <typename T>
		T Lerp(const T& A, const T& B, double Weight)
		{
			return PCGExTypeOps::FTypeOps<T>::Lerp(A, B, Weight);
		}

		template <typename T>
		T Min(const T& A, const T& B, double /*Weight*/)
		{
			return PCGExTypeOps::FTypeOps<T>::Min(A, B);
		}

		template <typename T>
		T Max(const T& A, const T& B, double /*Weight*/)
		{
			return PCGExTypeOps::FTypeOps<T>::Max(A, B);
		}

		template <typename T>
		T Average(const T& A, const T& B, double /*Weight*/)
		{
			return PCGExTypeOps::FTypeOps<T>::Average(A, B);
		}

		template <typename T>
		T WeightedAdd(const T& A, const T& B, double Weight)
		{
			return PCGExTypeOps::FTypeOps<T>::WeightedAdd(A, B, Weight);
		}

		template <typename T>
		T WeightedSub(const T& A, const T& B, double Weight)
		{
			return PCGExTypeOps::FTypeOps<T>::WeightedSub(A, B, Weight);
		}

		template <typename T>
		T CopyA(const T& A, const T& /*B*/, double /*Weight*/)
		{
			return A;
		}

		template <typename T>
		T CopyB(const T& /*A*/, const T& B, double /*Weight*/)
		{
			return B;
		}

		template <typename T>
		T UnsignedMin(const T& A, const T& B, double /*Weight*/)
		{
			return PCGExTypeOps::FTypeOps<T>::UnsignedMin(A, B);
		}

		template <typename T>
		T UnsignedMax(const T& A, const T& B, double /*Weight*/)
		{
			return PCGExTypeOps::FTypeOps<T>::UnsignedMax(A, B);
		}

		template <typename T>
		T AbsoluteMin(const T& A, const T& B, double /*Weight*/)
		{
			return PCGExTypeOps::FTypeOps<T>::AbsoluteMin(A, B);
		}

		template <typename T>
		T AbsoluteMax(const T& A, const T& B, double /*Weight*/)
		{
			return PCGExTypeOps::FTypeOps<T>::AbsoluteMax(A, B);
		}

		template <typename T>
		T NaiveHash(const T& A, const T& B, double /*Weight*/)
		{
			return PCGExTypeOps::FTypeOps<T>::NaiveHash(A, B);
		}

		template <typename T>
		T UnsignedHash(const T& A, const T& B, double /*Weight*/)
		{
			return PCGExTypeOps::FTypeOps<T>::UnsignedHash(A, B);
		}

		template <typename T>
		T ModSimple(const T& A, const T& /*B*/, double Weight)
		{
			return PCGExTypeOps::FTypeOps<T>::ModSimple(A, Weight);
		}

		template <typename T>
		T ModComplex(const T& A, const T& B, double /*Weight*/)
		{
			return PCGExTypeOps::FTypeOps<T>::ModComplex(A, B);
		}

		template <typename T>
		T Weight(const T& A, const T& B, double W)
		{
			return PCGExTypeOps::FTypeOps<T>::Weight(A, B, W);
		}

		template <typename T>
		T None(const T& A, const T& /*B*/, double /*Weight*/)
		{
			return A;
		}

		// Get blend function by mode
		template <typename T>
		typename TBlendOperationImpl<T>::BlendFn GetBlendFunction(EPCGExABBlendingType Mode)
		{
			switch (Mode)
			{
			case EPCGExABBlendingType::Add: return &Add<T>;
			case EPCGExABBlendingType::Subtract: return &Sub<T>;
			case EPCGExABBlendingType::Multiply: return &Mult<T>;
			case EPCGExABBlendingType::Divide: return &Div<T>;
			case EPCGExABBlendingType::Lerp: return &Lerp<T>;
			case EPCGExABBlendingType::Min: return &Min<T>;
			case EPCGExABBlendingType::Max: return &Max<T>;
			case EPCGExABBlendingType::Average: return &Average<T>;
			case EPCGExABBlendingType::WeightedAdd: return &WeightedAdd<T>;
			case EPCGExABBlendingType::WeightedSubtract: return &WeightedSub<T>;
			case EPCGExABBlendingType::CopyTarget: return &CopyA<T>;
			case EPCGExABBlendingType::CopySource: return &CopyB<T>;
			case EPCGExABBlendingType::UnsignedMin: return &UnsignedMin<T>;
			case EPCGExABBlendingType::UnsignedMax: return &UnsignedMax<T>;
			case EPCGExABBlendingType::AbsoluteMin: return &AbsoluteMin<T>;
			case EPCGExABBlendingType::AbsoluteMax: return &AbsoluteMax<T>;
			case EPCGExABBlendingType::Hash: return &NaiveHash<T>;
			case EPCGExABBlendingType::UnsignedHash: return &UnsignedHash<T>;
			case EPCGExABBlendingType::Mod: return &ModSimple<T>;
			case EPCGExABBlendingType::ModCW: return &ModComplex<T>;
			case EPCGExABBlendingType::Weight: return &Weight<T>;
			case EPCGExABBlendingType::None:
			default: return &None<T>;
			}
		}
	}

	/**
	 * FBlendOperationFactory - Creates blend operations with runtime dispatch
	 * 
	 * Single entry point for creating blend operations. Uses switch on type
	 * to dispatch to the appropriate TBlendOperationImpl<T> constructor.
	 * 
	 * Template instantiation happens here (14 times), not at every call site.
	 */
	class PCGEXTENDEDTOOLKIT_API FBlendOperationFactory
	{
	public:
		// Create a blend operation for runtime type and mode
		static TSharedPtr<IBlendOperation> Create(
			EPCGMetadataTypes WorkingType,
			EPCGExABBlendingType BlendMode,
			bool bResetForMultiBlend = true);

		// Compile-time factory for known type
		template <typename T>
		static TSharedPtr<IBlendOperation> CreateTyped(
			EPCGExABBlendingType BlendMode,
			bool bResetForMultiBlend = true)
		{
			return MakeShared<TBlendOperationImpl<T>>(
				BlendMode,
				bResetForMultiBlend,
				BlendFunctions::GetBlendFunction<T>(BlendMode));
		}
	};

	/**
	 * FProxyBlender - Simplified blender using type-erased operations
	 * 
	 * Replaces the old IProxyDataBlender hierarchy entirely.
	 * Uses runtime type information instead of template specialization.
	 */
	class PCGEXTENDEDTOOLKIT_API FProxyBlender
	{
	public:
		// Buffer accessors
		using GetFn = void(*)(const void* Buffer, int32 Index, void* OutValue);
		using SetFn = void(*)(void* Buffer, int32 Index, const void* Value);

	private:
		// Buffer pointers
		void* BufferA = nullptr;
		void* BufferB = nullptr;
		void* BufferC = nullptr; // Output

		// Buffer accessors
		GetFn GetA = nullptr;
		GetFn GetB = nullptr;
		SetFn SetC = nullptr;

		// Blend operation
		TSharedPtr<IBlendOperation> Operation;

		// Working type info
		EPCGMetadataTypes WorkingType = EPCGMetadataTypes::Unknown;
		int32 ValueSize = 0;
		int32 ValueAlignment = 0;

	public:
		FProxyBlender() = default;

		// Initialize with buffers and operation
		bool Init(
			void* InBufferA, GetFn InGetA,
			void* InBufferB, GetFn InGetB,
			void* InBufferC, SetFn InSetC,
			TSharedPtr<IBlendOperation> InOperation);

		// Single blend operation
		FORCEINLINE void Blend(int32 IdxA, int32 IdxB, int32 IdxC, double Weight) const
		{
			// Stack buffer for intermediate values
			alignas(16) uint8 ValA[sizeof(FTransform)];
			alignas(16) uint8 ValB[sizeof(FTransform)];
			alignas(16) uint8 ValC[sizeof(FTransform)];

			GetA(BufferA, IdxA, ValA);
			GetB(BufferB, IdxB, ValB);
			Operation->Blend(ValA, ValB, Weight, ValC);
			SetC(BufferC, IdxC, ValC);
		}

		// Multi-blend with accumulation
		void BlendMulti(
			const TArray<int32>& SourceIndices,
			const TArray<double>& Weights,
			int32 TargetIdx) const;

		// Properties
		bool IsValid() const { return Operation.IsValid(); }
		EPCGMetadataTypes GetWorkingType() const { return WorkingType; }
		EPCGExABBlendingType GetBlendMode() const { return Operation ? Operation->GetBlendMode() : EPCGExABBlendingType::None; }
	};

	/**
	 * FBlenderPool - Caches blend operations for reuse
	 * 
	 * Avoids repeated allocations by caching commonly used blend operations.
	 * Thread-safe through use of shared pointers.
	 */
	class PCGEXTENDEDTOOLKIT_API FBlenderPool
	{
	public:
		// Get or create a blend operation
		TSharedPtr<IBlendOperation> Get(
			EPCGMetadataTypes WorkingType,
			EPCGExABBlendingType BlendMode,
			bool bResetForMultiBlend = true);

		// Clear all cached operations
		void Clear();

		// Singleton access
		static FBlenderPool& GetGlobal();

	private:
		// Cache key
		struct FKey
		{
			EPCGMetadataTypes Type;
			EPCGExABBlendingType Mode;
			bool bReset;

			bool operator==(const FKey& Other) const
			{
				return Type == Other.Type && Mode == Other.Mode && bReset == Other.bReset;
			}
		};

		friend uint32 GetTypeHash(const FKey& Key)
		{
			return HashCombine(
				GetTypeHash(static_cast<uint8>(Key.Type)),
				HashCombine(GetTypeHash(static_cast<uint8>(Key.Mode)), GetTypeHash(Key.bReset)));
		}

		TMap<FKey, TSharedPtr<IBlendOperation>> Cache;
		FCriticalSection CacheLock;
	};
}
