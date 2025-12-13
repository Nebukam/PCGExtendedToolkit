// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/BlendOperations/PCGExBlendOperations.h"

// Include the blend mode enum - adjust path as needed
// #include "Data/Blending/PCGExDataBlending.h"

namespace PCGExDataBlending
{
	// === FBlendOperationFactory implementation ===

	TSharedPtr<IBlendOperation> FBlendOperationFactory::Create(
		EPCGMetadataTypes WorkingType,
		EPCGExABBlendingType BlendMode,
		bool bResetForMultiBlend)
	{
		switch (WorkingType)
		{
		case EPCGMetadataTypes::Boolean: return CreateTyped<bool>(BlendMode, bResetForMultiBlend);
		case EPCGMetadataTypes::Integer32: return CreateTyped<int32>(BlendMode, bResetForMultiBlend);
		case EPCGMetadataTypes::Integer64: return CreateTyped<int64>(BlendMode, bResetForMultiBlend);
		case EPCGMetadataTypes::Float: return CreateTyped<float>(BlendMode, bResetForMultiBlend);
		case EPCGMetadataTypes::Double: return CreateTyped<double>(BlendMode, bResetForMultiBlend);
		case EPCGMetadataTypes::Vector2: return CreateTyped<FVector2D>(BlendMode, bResetForMultiBlend);
		case EPCGMetadataTypes::Vector: return CreateTyped<FVector>(BlendMode, bResetForMultiBlend);
		case EPCGMetadataTypes::Vector4: return CreateTyped<FVector4>(BlendMode, bResetForMultiBlend);
		case EPCGMetadataTypes::Quaternion: return CreateTyped<FQuat>(BlendMode, bResetForMultiBlend);
		case EPCGMetadataTypes::Rotator: return CreateTyped<FRotator>(BlendMode, bResetForMultiBlend);
		case EPCGMetadataTypes::Transform: return CreateTyped<FTransform>(BlendMode, bResetForMultiBlend);
		case EPCGMetadataTypes::String: return CreateTyped<FString>(BlendMode, bResetForMultiBlend);
		case EPCGMetadataTypes::Name: return CreateTyped<FName>(BlendMode, bResetForMultiBlend);
		case EPCGMetadataTypes::SoftObjectPath: return CreateTyped<FSoftObjectPath>(BlendMode, bResetForMultiBlend);
		case EPCGMetadataTypes::SoftClassPath: return CreateTyped<FSoftClassPath>(BlendMode, bResetForMultiBlend);
		default: return nullptr;
		}
	}

	// === FProxyBlender implementation ===

	bool FProxyBlender::Init(
		void* InBufferA, GetFn InGetA,
		void* InBufferB, GetFn InGetB,
		void* InBufferC, SetFn InSetC,
		TSharedPtr<IBlendOperation> InOperation)
	{
		if (!InOperation.IsValid())
		{
			return false;
		}

		BufferA = InBufferA;
		BufferB = InBufferB;
		BufferC = InBufferC;
		GetA = InGetA;
		GetB = InGetB;
		SetC = InSetC;
		Operation = InOperation;

		WorkingType = Operation->GetWorkingType();
		ValueSize = Operation->GetValueSize();
		ValueAlignment = Operation->GetValueAlignment();

		return true;
	}

	void FProxyBlender::BlendMulti(
		const TArray<int32>& SourceIndices,
		const TArray<double>& Weights,
		int32 TargetIdx) const
	{
		if (!Operation.IsValid() || SourceIndices.Num() == 0)
		{
			return;
		}

		// Stack buffer for accumulator
		alignas(16) uint8 Accumulator[sizeof(FTransform)];
		alignas(16) uint8 TempValue[sizeof(FTransform)];

		// Initialize accumulator
		Operation->BeginMulti(Accumulator);

		// Accumulate all sources
		double TotalWeight = 0.0;
		const int32 NumSources = SourceIndices.Num();

		for (int32 i = 0; i < NumSources; ++i)
		{
			const double Weight = (i < Weights.Num()) ? Weights[i] : 1.0;
			GetA(BufferA, SourceIndices[i], TempValue);
			Operation->Accumulate(TempValue, Accumulator, Weight);
			TotalWeight += Weight;
		}

		// Finalize and store
		Operation->EndMulti(Accumulator, TotalWeight, NumSources);
		SetC(BufferC, TargetIdx, Accumulator);
	}

	// === FBlenderPool implementation ===

	TSharedPtr<IBlendOperation> FBlenderPool::Get(
		EPCGMetadataTypes WorkingType,
		EPCGExABBlendingType BlendMode,
		bool bResetForMultiBlend)
	{
		FKey Key{WorkingType, BlendMode, bResetForMultiBlend};

		{
			FScopeLock Lock(&CacheLock);
			if (TSharedPtr<IBlendOperation>* Found = Cache.Find(Key))
			{
				return *Found;
			}
		}

		// Create new operation outside lock
		TSharedPtr<IBlendOperation> NewOp = FBlendOperationFactory::Create(WorkingType, BlendMode, bResetForMultiBlend);

		if (NewOp.IsValid())
		{
			FScopeLock Lock(&CacheLock);
			// Check again in case another thread added it
			if (TSharedPtr<IBlendOperation>* Found = Cache.Find(Key))
			{
				return *Found;
			}
			Cache.Add(Key, NewOp);
		}

		return NewOp;
	}

	void FBlenderPool::Clear()
	{
		FScopeLock Lock(&CacheLock);
		Cache.Empty();
	}

	FBlenderPool& FBlenderPool::GetGlobal()
	{
		static FBlenderPool Instance;
		return Instance;
	}

	// === Explicit template instantiations ===
	// Only 14 instantiations instead of 616!

	template class TBlendOperationImpl<bool>;
	template class TBlendOperationImpl<int32>;
	template class TBlendOperationImpl<int64>;
	template class TBlendOperationImpl<float>;
	template class TBlendOperationImpl<double>;
	template class TBlendOperationImpl<FVector2D>;
	template class TBlendOperationImpl<FVector>;
	template class TBlendOperationImpl<FVector4>;
	template class TBlendOperationImpl<FQuat>;
	template class TBlendOperationImpl<FRotator>;
	template class TBlendOperationImpl<FTransform>;
	template class TBlendOperationImpl<FString>;
	template class TBlendOperationImpl<FName>;
	template class TBlendOperationImpl<FSoftObjectPath>;
	template class TBlendOperationImpl<FSoftClassPath>;

	// Explicit instantiation of blend function getters
#define INST_BLEND_FUNC_GETTER(TYPE) \
		template TBlendOperationImpl<TYPE>::BlendFn BlendFunctions::GetBlendFunction<TYPE>(EPCGExABBlendingType);

	INST_BLEND_FUNC_GETTER(bool)
	INST_BLEND_FUNC_GETTER(int32)
	INST_BLEND_FUNC_GETTER(int64)
	INST_BLEND_FUNC_GETTER(float)
	INST_BLEND_FUNC_GETTER(double)
	INST_BLEND_FUNC_GETTER(FVector2D)
	INST_BLEND_FUNC_GETTER(FVector)
	INST_BLEND_FUNC_GETTER(FVector4)
	INST_BLEND_FUNC_GETTER(FQuat)
	INST_BLEND_FUNC_GETTER(FRotator)
	INST_BLEND_FUNC_GETTER(FTransform)
	INST_BLEND_FUNC_GETTER(FString)
	INST_BLEND_FUNC_GETTER(FName)
	INST_BLEND_FUNC_GETTER(FSoftObjectPath)
	INST_BLEND_FUNC_GETTER(FSoftClassPath)

#undef INST_BLEND_FUNC_GETTER
}
