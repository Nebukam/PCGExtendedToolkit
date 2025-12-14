// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExBlendOperations.h"

#include "PCGEx.h"

namespace PCGExBlending
{
#pragma region FScopedTypedValue

	// FScopedTypedValue implementation

	FScopedTypedValue::FScopedTypedValue(EPCGMetadataTypes InType)
		: Type(InType), bConstructed(false)
	{
		if (NeedsLifecycleManagement(Type))
		{
			switch (Type)
			{
			case EPCGMetadataTypes::String: new(Storage) FString();
				break;
			case EPCGMetadataTypes::Name: new(Storage) FName();
				break;
			case EPCGMetadataTypes::SoftObjectPath: new(Storage) FSoftObjectPath();
				break;
			case EPCGMetadataTypes::SoftClassPath: new(Storage) FSoftClassPath();
				break;
			default: break;
			}
			bConstructed = true;
		}
		else
		{
			// Zero-initialize POD types
			FMemory::Memzero(Storage, BufferSize);
		}
	}

	FScopedTypedValue::~FScopedTypedValue()
	{
		Destruct();
	}

	FScopedTypedValue::FScopedTypedValue(FScopedTypedValue&& Other) noexcept
		: Type(Other.Type)
		  , bConstructed(Other.bConstructed)
	{
		if (bConstructed && NeedsLifecycleManagement(Type))
		{
			// Move construct complex types
			switch (Type)
			{
			case EPCGMetadataTypes::String: new(Storage) FString(MoveTemp(*reinterpret_cast<FString*>(Other.Storage)));
				break;
			case EPCGMetadataTypes::Name: new(Storage) FName(MoveTemp(*reinterpret_cast<FName*>(Other.Storage)));
				break;
			case EPCGMetadataTypes::SoftObjectPath: new(Storage) FSoftObjectPath(MoveTemp(*reinterpret_cast<FSoftObjectPath*>(Other.Storage)));
				break;
			case EPCGMetadataTypes::SoftClassPath: new(Storage) FSoftClassPath(MoveTemp(*reinterpret_cast<FSoftClassPath*>(Other.Storage)));
				break;
			default: FMemory::Memcpy(Storage, Other.Storage, BufferSize);
				break;
			}
		}
		else
		{
			FMemory::Memcpy(Storage, Other.Storage, BufferSize);
		}

		// Mark other as not constructed to prevent double destruction
		Other.bConstructed = false;
		Other.Type = EPCGMetadataTypes::Unknown;
	}

	void FScopedTypedValue::Destruct()
	{
		if (bConstructed && NeedsLifecycleManagement(Type))
		{
			switch (Type)
			{
			case EPCGMetadataTypes::String: reinterpret_cast<FString*>(Storage)->~FString();
				break;
			case EPCGMetadataTypes::Name: reinterpret_cast<FName*>(Storage)->~FName();
				break;
			case EPCGMetadataTypes::SoftObjectPath: reinterpret_cast<FSoftObjectPath*>(Storage)->~FSoftObjectPath();
				break;
			case EPCGMetadataTypes::SoftClassPath: reinterpret_cast<FSoftClassPath*>(Storage)->~FSoftClassPath();
				break;
			default: break;
			}
		}
		bConstructed = false;
	}

	void FScopedTypedValue::Initialize(EPCGMetadataTypes NewType)
	{
		// Destruct old value if needed
		Destruct();

		Type = NewType;

		if (NeedsLifecycleManagement(Type))
		{
			switch (Type)
			{
			case EPCGMetadataTypes::String: new(Storage) FString();
				break;
			case EPCGMetadataTypes::Name: new(Storage) FName();
				break;
			case EPCGMetadataTypes::SoftObjectPath: new(Storage) FSoftObjectPath();
				break;
			case EPCGMetadataTypes::SoftClassPath: new(Storage) FSoftClassPath();
				break;
			default: break;
			}
			bConstructed = true;
		}
		else
		{
			FMemory::Memzero(Storage, BufferSize);
		}
	}

	bool FScopedTypedValue::NeedsLifecycleManagement(EPCGMetadataTypes InType)
	{
		switch (InType)
		{
		case EPCGMetadataTypes::String:
		case EPCGMetadataTypes::Name:
		case EPCGMetadataTypes::SoftObjectPath:
		case EPCGMetadataTypes::SoftClassPath: return true;
		default: return false;
		}
	}

	int32 FScopedTypedValue::GetTypeSize(EPCGMetadataTypes InType)
	{
		switch (InType)
		{
		case EPCGMetadataTypes::Boolean: return sizeof(bool);
		case EPCGMetadataTypes::Integer32: return sizeof(int32);
		case EPCGMetadataTypes::Integer64: return sizeof(int64);
		case EPCGMetadataTypes::Float: return sizeof(float);
		case EPCGMetadataTypes::Double: return sizeof(double);
		case EPCGMetadataTypes::Vector2: return sizeof(FVector2D);
		case EPCGMetadataTypes::Vector: return sizeof(FVector);
		case EPCGMetadataTypes::Vector4: return sizeof(FVector4);
		case EPCGMetadataTypes::Quaternion: return sizeof(FQuat);
		case EPCGMetadataTypes::Rotator: return sizeof(FRotator);
		case EPCGMetadataTypes::Transform: return sizeof(FTransform);
		case EPCGMetadataTypes::String: return sizeof(FString);
		case EPCGMetadataTypes::Name: return sizeof(FName);
		case EPCGMetadataTypes::SoftObjectPath: return sizeof(FSoftObjectPath);
		case EPCGMetadataTypes::SoftClassPath: return sizeof(FSoftClassPath);
		default: return 0;
		}
	}

	IBlendOperation::IBlendOperation(const EPCGExABBlendingType InMode, const bool bInResetForMulti)
		: Mode(InMode),
		  bResetForMulti(bInResetForMulti),
		  bInitWithSource(
			  InMode == EPCGExABBlendingType::Min ||
			  InMode == EPCGExABBlendingType::Max ||
			  InMode == EPCGExABBlendingType::UnsignedMin ||
			  InMode == EPCGExABBlendingType::UnsignedMax ||
			  InMode == EPCGExABBlendingType::AbsoluteMin ||
			  InMode == EPCGExABBlendingType::AbsoluteMax ||
			  InMode == EPCGExABBlendingType::Hash ||
			  InMode == EPCGExABBlendingType::Hash),
		  bConsiderOriginalValue(
			  InMode == EPCGExABBlendingType::Average ||
			  InMode == EPCGExABBlendingType::Add ||
			  InMode == EPCGExABBlendingType::Subtract ||
			  InMode == EPCGExABBlendingType::Weight ||
			  InMode == EPCGExABBlendingType::WeightedAdd ||
			  InMode == EPCGExABBlendingType::WeightedSubtract)
	{
	}

	void IBlendOperation::Blend(const void* A, const void* B, double Weight, void* Out) const
	{
		BlendFunc(A, B, Weight, Out);
	}

	void IBlendOperation::BeginMulti(void* Accumulator, const void* InitialValue, PCGEx::FOpStats& OutTracker) const
	{
		if (bInitWithSource)
		{
			// These modes require the first operation to be a copy of the first blended value
			// before they can be properly blended -- this should be handled by the blend op?
			OutTracker.Count = -1;
		}
		else if (bConsiderOriginalValue)
		{
			// Some BlendModes can leverage this
			if (bResetForMulti)
			{
				InitDefault(Accumulator);
			}
			else
			{
				// Otherwise, bump up original count so EndBlend can account for pre-existing value as "one blend step"
				OutTracker.Count = 1;
				OutTracker.TotalWeight = 1;
			}
		}
		/*
		if (InitialValue)
		{
			// Copy initial value
			CopyValue(InitialValue, Accumulator);
		}
		*/
	}

	void IBlendOperation::Accumulate(const void* Source, void* Accumulator, double Weight) const
	{
		AccumulateFunc(Accumulator, Source, Weight, Accumulator); 
	}

	void IBlendOperation::EndMulti(void* Accumulator, double TotalWeight, int32 Count) const
	{
		FinalizeFunc(Accumulator, TotalWeight, Count);
	}

#pragma endregion

	// FBlendOperationFactory implementation

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

	// FBlenderPool implementation

	TSharedPtr<IBlendOperation> FBlenderPool::Get(
		EPCGMetadataTypes WorkingType,
		EPCGExABBlendingType BlendMode,
		bool bResetForMultiBlend)
	{
		FKey Key{WorkingType, BlendMode, bResetForMultiBlend};

		{
			FScopeLock Lock(&CacheLock);
			if (TSharedPtr<IBlendOperation>* Found = Cache.Find(Key)) { return *Found; }
		}

		// Create new operation outside lock
		TSharedPtr<IBlendOperation> NewOp = FBlendOperationFactory::Create(WorkingType, BlendMode, bResetForMultiBlend);

		if (NewOp.IsValid())
		{
			FScopeLock Lock(&CacheLock);
			// Check again in case another thread added it
			if (TSharedPtr<IBlendOperation>* Found = Cache.Find(Key)) { return *Found; }
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

	// Explicit template instantiations

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
	template FBlendFn BlendFunctions::GetBlendFunction<TYPE>(EPCGExABBlendingType); \
	template FFinalizeFn BlendFunctions::GetFinalizeFunction<TYPE>(EPCGExABBlendingType);

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
