// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExTypeErasedProxy.h"
#include "Data/PCGExData.h"
#include "Data/PCGExProxyData.h"

namespace PCGExData
{
	//////////////////////////////////////////////////////////////////////////
	// Accessor Function Templates
	//////////////////////////////////////////////////////////////////////////
	
	namespace ProxyAccessors
	{
		// === Attribute Buffer Accessors ===
		
		template<typename T_REAL, typename T_WORKING>
		void BufferGet(const FTypeErasedProxy* Self, int32 Index, void* OutValue)
		{
			const TBuffer<T_REAL>* Buffer = static_cast<const TBuffer<T_REAL>*>(Self->GetBuffer().Get());
			const T_REAL& RealValue = Buffer->Read(Index);
			
			if constexpr (std::is_same_v<T_REAL, T_WORKING>)
			{
				*static_cast<T_WORKING*>(OutValue) = RealValue;
			}
			else
			{
				*static_cast<T_WORKING*>(OutValue) = PCGExTypeOps::FTypeOps<T_REAL>::template ConvertTo<T_WORKING>(RealValue);
			}
		}
		
		template<typename T_REAL, typename T_WORKING>
		void BufferSet(const FTypeErasedProxy* Self, int32 Index, const void* Value)
		{
			TBuffer<T_REAL>* Buffer = static_cast<TBuffer<T_REAL>*>(const_cast<IBuffer*>(Self->GetBuffer().Get()));
			const T_WORKING& WorkingValue = *static_cast<const T_WORKING*>(Value);
			
			if constexpr (std::is_same_v<T_REAL, T_WORKING>)
			{
				Buffer->SetValue(Index, WorkingValue);
			}
			else
			{
				T_REAL RealValue = PCGExTypeOps::FTypeOps<T_WORKING>::template ConvertTo<T_REAL>(WorkingValue);
				Buffer->SetValue(Index, RealValue);
			}
		}
		
		template<typename T_REAL, typename T_WORKING>
		void BufferGetCurrent(const FTypeErasedProxy* Self, int32 Index, void* OutValue)
		{
			TBuffer<T_REAL>* Buffer = static_cast<TBuffer<T_REAL>*>(const_cast<IBuffer*>(Self->GetBuffer().Get()));
			const T_REAL& RealValue = Buffer->GetValue(Index);
			
			if constexpr (std::is_same_v<T_REAL, T_WORKING>)
			{
				*static_cast<T_WORKING*>(OutValue) = RealValue;
			}
			else
			{
				*static_cast<T_WORKING*>(OutValue) = PCGExTypeOps::FTypeOps<T_REAL>::template ConvertTo<T_WORKING>(RealValue);
			}
		}
		
		template<typename T_REAL>
		PCGExValueHash BufferGetHash(const FTypeErasedProxy* Self, int32 Index)
		{
			const TBuffer<T_REAL>* Buffer = static_cast<const TBuffer<T_REAL>*>(Self->GetBuffer().Get());
			return Buffer->ReadValueHash(Index);
		}
		
		// === Constant Accessors ===
		
		template<typename T_WORKING>
		void ConstantGet(const FTypeErasedProxy* Self, int32 /*Index*/, void* OutValue)
		{
			// Constant is stored in ConstantStorage, already in working type
			FMemory::Memcpy(OutValue, Self->GetConstantStorage(), sizeof(T_WORKING));
		}
		
		template<typename T_WORKING>
		void ConstantSet(const FTypeErasedProxy* /*Self*/, int32 /*Index*/, const void* /*Value*/)
		{
			// Cannot set a constant - this should not be called
			checkNoEntry();
		}
		
		template<typename T_WORKING>
		uint64 ConstantGetHash(const FTypeErasedProxy* Self, int32 /*Index*/)
		{
			const T_WORKING* Value = reinterpret_cast<const T_WORKING*>(Self->GetConstantStorage());
			return PCGExTypeOps::FTypeOps<T_WORKING>::Hash(*Value);
		}
		
		// === Point Property Accessors ===
		// These need to be specialized per property type
		
		template<typename T_REAL, typename T_WORKING, int32 PropertyEnum>
		void PointPropertyGet(const FTypeErasedProxy* Self, int32 Index, void* OutValue)
		{
			
		}
		
		template<typename T_REAL, typename T_WORKING, int32 PropertyEnum>
		void PointPropertySet(const FTypeErasedProxy* Self, int32 Index, const void* Value)
		{
			
		}
		
		// Generic fallback implementation
		template<typename T_REAL, typename T_WORKING>
		void GenericPointPropertyGet(const FTypeErasedProxy* Self, int32 Index, void* OutValue, int32 PropertyEnum)
		{
			// This requires access to point data - implementation depends on EPCGPointProperties
			// For now, provide a stub that can be extended
			UPCGBasePointData* PointData = Self->GetPointData();
			if (!PointData) { return; }
			
			// The actual implementation would switch on PropertyEnum and access the appropriate property
			// This is a placeholder - real implementation needs PCG headers
			*static_cast<T_WORKING*>(OutValue) = T_WORKING{};
		}
		
		// === Function Pointer Tables ===
		
		// Get accessor function for buffer source
		template<typename T_REAL>
		FTypeErasedProxy::FGetFn GetBufferGetFn(EPCGMetadataTypes WorkingType)
		{
			switch (WorkingType)
			{
			case EPCGMetadataTypes::Boolean: return &BufferGet<T_REAL, bool>;
			case EPCGMetadataTypes::Integer32: return &BufferGet<T_REAL, int32>;
			case EPCGMetadataTypes::Integer64: return &BufferGet<T_REAL, int64>;
			case EPCGMetadataTypes::Float: return &BufferGet<T_REAL, float>;
			case EPCGMetadataTypes::Double: return &BufferGet<T_REAL, double>;
			case EPCGMetadataTypes::Vector2: return &BufferGet<T_REAL, FVector2D>;
			case EPCGMetadataTypes::Vector: return &BufferGet<T_REAL, FVector>;
			case EPCGMetadataTypes::Vector4: return &BufferGet<T_REAL, FVector4>;
			case EPCGMetadataTypes::Quaternion: return &BufferGet<T_REAL, FQuat>;
			case EPCGMetadataTypes::Rotator: return &BufferGet<T_REAL, FRotator>;
			case EPCGMetadataTypes::Transform: return &BufferGet<T_REAL, FTransform>;
			case EPCGMetadataTypes::String: return &BufferGet<T_REAL, FString>;
			case EPCGMetadataTypes::Name: return &BufferGet<T_REAL, FName>;
			case EPCGMetadataTypes::SoftObjectPath: return &BufferGet<T_REAL, FSoftObjectPath>;
			case EPCGMetadataTypes::SoftClassPath: return &BufferGet<T_REAL, FSoftClassPath>;
			default: return nullptr;
			}
		}
		
		template<typename T_REAL>
		FTypeErasedProxy::FSetFn GetBufferSetFn(EPCGMetadataTypes WorkingType)
		{
			switch (WorkingType)
			{
			case EPCGMetadataTypes::Boolean: return &BufferSet<T_REAL, bool>;
			case EPCGMetadataTypes::Integer32: return &BufferSet<T_REAL, int32>;
			case EPCGMetadataTypes::Integer64: return &BufferSet<T_REAL, int64>;
			case EPCGMetadataTypes::Float: return &BufferSet<T_REAL, float>;
			case EPCGMetadataTypes::Double: return &BufferSet<T_REAL, double>;
			case EPCGMetadataTypes::Vector2: return &BufferSet<T_REAL, FVector2D>;
			case EPCGMetadataTypes::Vector: return &BufferSet<T_REAL, FVector>;
			case EPCGMetadataTypes::Vector4: return &BufferSet<T_REAL, FVector4>;
			case EPCGMetadataTypes::Quaternion: return &BufferSet<T_REAL, FQuat>;
			case EPCGMetadataTypes::Rotator: return &BufferSet<T_REAL, FRotator>;
			case EPCGMetadataTypes::Transform: return &BufferSet<T_REAL, FTransform>;
			case EPCGMetadataTypes::String: return &BufferSet<T_REAL, FString>;
			case EPCGMetadataTypes::Name: return &BufferSet<T_REAL, FName>;
			case EPCGMetadataTypes::SoftObjectPath: return &BufferSet<T_REAL, FSoftObjectPath>;
			case EPCGMetadataTypes::SoftClassPath: return &BufferSet<T_REAL, FSoftClassPath>;
			default: return nullptr;
			}
		}
		
		template<typename T_REAL>
		FTypeErasedProxy::FGetCurrentFn GetBufferGetCurrentFn(EPCGMetadataTypes WorkingType)
		{
			switch (WorkingType)
			{
			case EPCGMetadataTypes::Boolean: return &BufferGetCurrent<T_REAL, bool>;
			case EPCGMetadataTypes::Integer32: return &BufferGetCurrent<T_REAL, int32>;
			case EPCGMetadataTypes::Integer64: return &BufferGetCurrent<T_REAL, int64>;
			case EPCGMetadataTypes::Float: return &BufferGetCurrent<T_REAL, float>;
			case EPCGMetadataTypes::Double: return &BufferGetCurrent<T_REAL, double>;
			case EPCGMetadataTypes::Vector2: return &BufferGetCurrent<T_REAL, FVector2D>;
			case EPCGMetadataTypes::Vector: return &BufferGetCurrent<T_REAL, FVector>;
			case EPCGMetadataTypes::Vector4: return &BufferGetCurrent<T_REAL, FVector4>;
			case EPCGMetadataTypes::Quaternion: return &BufferGetCurrent<T_REAL, FQuat>;
			case EPCGMetadataTypes::Rotator: return &BufferGetCurrent<T_REAL, FRotator>;
			case EPCGMetadataTypes::Transform: return &BufferGetCurrent<T_REAL, FTransform>;
			case EPCGMetadataTypes::String: return &BufferGetCurrent<T_REAL, FString>;
			case EPCGMetadataTypes::Name: return &BufferGetCurrent<T_REAL, FName>;
			case EPCGMetadataTypes::SoftObjectPath: return &BufferGetCurrent<T_REAL, FSoftObjectPath>;
			case EPCGMetadataTypes::SoftClassPath: return &BufferGetCurrent<T_REAL, FSoftClassPath>;
			default: return nullptr;
			}
		}
		
		// Get constant accessor
		FTypeErasedProxy::FGetFn GetConstantGetFn(EPCGMetadataTypes WorkingType)
		{
			switch (WorkingType)
			{
			case EPCGMetadataTypes::Boolean: return &ConstantGet<bool>;
			case EPCGMetadataTypes::Integer32: return &ConstantGet<int32>;
			case EPCGMetadataTypes::Integer64: return &ConstantGet<int64>;
			case EPCGMetadataTypes::Float: return &ConstantGet<float>;
			case EPCGMetadataTypes::Double: return &ConstantGet<double>;
			case EPCGMetadataTypes::Vector2: return &ConstantGet<FVector2D>;
			case EPCGMetadataTypes::Vector: return &ConstantGet<FVector>;
			case EPCGMetadataTypes::Vector4: return &ConstantGet<FVector4>;
			case EPCGMetadataTypes::Quaternion: return &ConstantGet<FQuat>;
			case EPCGMetadataTypes::Rotator: return &ConstantGet<FRotator>;
			case EPCGMetadataTypes::Transform: return &ConstantGet<FTransform>;
			case EPCGMetadataTypes::String: return &ConstantGet<FString>;
			case EPCGMetadataTypes::Name: return &ConstantGet<FName>;
			case EPCGMetadataTypes::SoftObjectPath: return &ConstantGet<FSoftObjectPath>;
			case EPCGMetadataTypes::SoftClassPath: return &ConstantGet<FSoftClassPath>;
			default: return nullptr;
			}
		}
		
		// Dispatch to get buffer accessors
		void GetBufferAccessors(
			EPCGMetadataTypes RealType,
			EPCGMetadataTypes WorkingType,
			FTypeErasedProxy::FGetFn& OutGetFn,
			FTypeErasedProxy::FSetFn& OutSetFn,
			FTypeErasedProxy::FGetCurrentFn& OutGetCurrentFn,
			FTypeErasedProxy::FGetHashFn& OutGetHashFn)
		{
			switch (RealType)
			{
			case EPCGMetadataTypes::Boolean:
				OutGetFn = GetBufferGetFn<bool>(WorkingType);
				OutSetFn = GetBufferSetFn<bool>(WorkingType);
				OutGetCurrentFn = GetBufferGetCurrentFn<bool>(WorkingType);
				OutGetHashFn = &BufferGetHash<bool>;
				break;
			case EPCGMetadataTypes::Integer32:
				OutGetFn = GetBufferGetFn<int32>(WorkingType);
				OutSetFn = GetBufferSetFn<int32>(WorkingType);
				OutGetCurrentFn = GetBufferGetCurrentFn<int32>(WorkingType);
				OutGetHashFn = &BufferGetHash<int32>;
				break;
			case EPCGMetadataTypes::Integer64:
				OutGetFn = GetBufferGetFn<int64>(WorkingType);
				OutSetFn = GetBufferSetFn<int64>(WorkingType);
				OutGetCurrentFn = GetBufferGetCurrentFn<int64>(WorkingType);
				OutGetHashFn = &BufferGetHash<int64>;
				break;
			case EPCGMetadataTypes::Float:
				OutGetFn = GetBufferGetFn<float>(WorkingType);
				OutSetFn = GetBufferSetFn<float>(WorkingType);
				OutGetCurrentFn = GetBufferGetCurrentFn<float>(WorkingType);
				OutGetHashFn = &BufferGetHash<float>;
				break;
			case EPCGMetadataTypes::Double:
				OutGetFn = GetBufferGetFn<double>(WorkingType);
				OutSetFn = GetBufferSetFn<double>(WorkingType);
				OutGetCurrentFn = GetBufferGetCurrentFn<double>(WorkingType);
				OutGetHashFn = &BufferGetHash<double>;
				break;
			case EPCGMetadataTypes::Vector2:
				OutGetFn = GetBufferGetFn<FVector2D>(WorkingType);
				OutSetFn = GetBufferSetFn<FVector2D>(WorkingType);
				OutGetCurrentFn = GetBufferGetCurrentFn<FVector2D>(WorkingType);
				OutGetHashFn = &BufferGetHash<FVector2D>;
				break;
			case EPCGMetadataTypes::Vector:
				OutGetFn = GetBufferGetFn<FVector>(WorkingType);
				OutSetFn = GetBufferSetFn<FVector>(WorkingType);
				OutGetCurrentFn = GetBufferGetCurrentFn<FVector>(WorkingType);
				OutGetHashFn = &BufferGetHash<FVector>;
				break;
			case EPCGMetadataTypes::Vector4:
				OutGetFn = GetBufferGetFn<FVector4>(WorkingType);
				OutSetFn = GetBufferSetFn<FVector4>(WorkingType);
				OutGetCurrentFn = GetBufferGetCurrentFn<FVector4>(WorkingType);
				OutGetHashFn = &BufferGetHash<FVector4>;
				break;
			case EPCGMetadataTypes::Quaternion:
				OutGetFn = GetBufferGetFn<FQuat>(WorkingType);
				OutSetFn = GetBufferSetFn<FQuat>(WorkingType);
				OutGetCurrentFn = GetBufferGetCurrentFn<FQuat>(WorkingType);
				OutGetHashFn = &BufferGetHash<FQuat>;
				break;
			case EPCGMetadataTypes::Rotator:
				OutGetFn = GetBufferGetFn<FRotator>(WorkingType);
				OutSetFn = GetBufferSetFn<FRotator>(WorkingType);
				OutGetCurrentFn = GetBufferGetCurrentFn<FRotator>(WorkingType);
				OutGetHashFn = &BufferGetHash<FRotator>;
				break;
			case EPCGMetadataTypes::Transform:
				OutGetFn = GetBufferGetFn<FTransform>(WorkingType);
				OutSetFn = GetBufferSetFn<FTransform>(WorkingType);
				OutGetCurrentFn = GetBufferGetCurrentFn<FTransform>(WorkingType);
				OutGetHashFn = &BufferGetHash<FTransform>;
				break;
			case EPCGMetadataTypes::String:
				OutGetFn = GetBufferGetFn<FString>(WorkingType);
				OutSetFn = GetBufferSetFn<FString>(WorkingType);
				OutGetCurrentFn = GetBufferGetCurrentFn<FString>(WorkingType);
				OutGetHashFn = &BufferGetHash<FString>;
				break;
			case EPCGMetadataTypes::Name:
				OutGetFn = GetBufferGetFn<FName>(WorkingType);
				OutSetFn = GetBufferSetFn<FName>(WorkingType);
				OutGetCurrentFn = GetBufferGetCurrentFn<FName>(WorkingType);
				OutGetHashFn = &BufferGetHash<FName>;
				break;
			case EPCGMetadataTypes::SoftObjectPath:
				OutGetFn = GetBufferGetFn<FSoftObjectPath>(WorkingType);
				OutSetFn = GetBufferSetFn<FSoftObjectPath>(WorkingType);
				OutGetCurrentFn = GetBufferGetCurrentFn<FSoftObjectPath>(WorkingType);
				OutGetHashFn = &BufferGetHash<FSoftObjectPath>;
				break;
			case EPCGMetadataTypes::SoftClassPath:
				OutGetFn = GetBufferGetFn<FSoftClassPath>(WorkingType);
				OutSetFn = GetBufferSetFn<FSoftClassPath>(WorkingType);
				OutGetCurrentFn = GetBufferGetCurrentFn<FSoftClassPath>(WorkingType);
				OutGetHashFn = &BufferGetHash<FSoftClassPath>;
				break;
			default:
				OutGetFn = nullptr;
				OutSetFn = nullptr;
				OutGetCurrentFn = nullptr;
				OutGetHashFn = nullptr;
				break;
			}
		}
	} // namespace ProxyAccessors

	//////////////////////////////////////////////////////////////////////////
	// FTypeErasedProxy Implementation
	//////////////////////////////////////////////////////////////////////////
	
	FTypeErasedProxy::FTypeErasedProxy()
	{
		FMemory::Memzero(ConstantStorage);
		PointData = nullptr;
	}
	
	FTypeErasedProxy::~FTypeErasedProxy()
	{
		// Clean up based on source type
		if (SourceType == EProxySourceType::Constant)
		{
			// Destruct the constant value if it's a non-trivial type
			if (WorkingTypeOps)
			{
				// For string types, we need to call destructor
				// This is safe because we used placement new
			}
		}
	}
	
	FTypeErasedProxy::FTypeErasedProxy(FTypeErasedProxy&& Other) noexcept
	{
		*this = MoveTemp(Other);
	}
	
	FTypeErasedProxy& FTypeErasedProxy::operator=(FTypeErasedProxy&& Other) noexcept
	{
		if (this != &Other)
		{
			SourceType = Other.SourceType;
			RealType = Other.RealType;
			WorkingType = Other.WorkingType;
			RealTypeOps = Other.RealTypeOps;
			WorkingTypeOps = Other.WorkingTypeOps;
			ToWorkingConverter = Other.ToWorkingConverter;
			FromWorkingConverter = Other.FromWorkingConverter;
			GetFn = Other.GetFn;
			SetFn = Other.SetFn;
			GetCurrentFn = Other.GetCurrentFn;
			GetHashFn = Other.GetHashFn;
			bWantsSubSelection = Other.bWantsSubSelection;
			SubSelectionIndex = Other.SubSelectionIndex;
			PropertyEnum = Other.PropertyEnum;
			BufferStorage = MoveTemp(Other.BufferStorage);
			FMemory::Memcpy(ConstantStorage, Other.ConstantStorage, sizeof(ConstantStorage));
			
			// Handle union
			switch (SourceType)
			{
			case EProxySourceType::AttributeBuffer:
				BufferPtr = &BufferStorage;
				break;
			case EProxySourceType::PointProperty:
			case EProxySourceType::PointExtraProperty:
				PointData = Other.PointData;
				break;
			case EProxySourceType::Constant:
				ConstantValuePtr = ConstantStorage;
				break;
			case EProxySourceType::DirectAttribute:
			case EProxySourceType::DirectDataAttribute:
				DirectAttribute = Other.DirectAttribute;
				break;
			default:
				break;
			}
			
			// Clear other
			Other.SourceType = EProxySourceType::None;
		}
		return *this;
	}

	void FTypeErasedProxy::Get(int32 Index, void* OutValue) const
	{
		checkSlow(GetFn);
		GetFn(this, Index, OutValue);
	}

	void FTypeErasedProxy::Set(int32 Index, const void* Value) const
	{
		checkSlow(SetFn);
		SetFn(this, Index, Value);
	}

	void FTypeErasedProxy::GetCurrent(int32 Index, void* OutValue) const
	{
		if (GetCurrentFn)
		{
			GetCurrentFn(this, Index, OutValue);
		}
		else
		{
			// Default: same as Get
			Get(Index, OutValue);
		}
	}

	uint64 FTypeErasedProxy::GetValueHash(int32 Index) const
	{
		if (GetHashFn)
		{
			return GetHashFn(this, Index);
		}
		return 0;
	}

	TSharedPtr<IBuffer> FTypeErasedProxy::GetBuffer() const
	{
		if (SourceType == EProxySourceType::AttributeBuffer)
		{
			return BufferStorage;
		}
		return nullptr;
	}
	
	UPCGBasePointData* FTypeErasedProxy::GetPointData() const
	{
		if (SourceType == EProxySourceType::PointProperty || SourceType == EProxySourceType::PointExtraProperty)
		{
			return PointData;
		}
		return nullptr;
	}
	
	void FTypeErasedProxy::SetSubSelection(int32 ComponentIndex)
	{
		bWantsSubSelection = (ComponentIndex >= 0);
		SubSelectionIndex = ComponentIndex;
	}
	
	void FTypeErasedProxy::ClearSubSelection()
	{
		bWantsSubSelection = false;
		SubSelectionIndex = -1;
	}
	
	bool FTypeErasedProxy::Validate(const FProxyDescriptor& InDescriptor) const
	{
		return InDescriptor.RealType == RealType && InDescriptor.WorkingType == WorkingType;
	}
	
	void FTypeErasedProxy::InitializeTypeOps()
	{
		RealTypeOps = PCGExTypeOps::FTypeOpsRegistry::Get(RealType);
		WorkingTypeOps = PCGExTypeOps::FTypeOpsRegistry::Get(WorkingType);
		
		if (RealType != WorkingType)
		{
			ToWorkingConverter = PCGExTypeOps::FConversionTable::GetConversionFn(RealType, WorkingType);
			FromWorkingConverter = PCGExTypeOps::FConversionTable::GetConversionFn(WorkingType, RealType);
		}
		else
		{
			ToWorkingConverter = nullptr;
			FromWorkingConverter = nullptr;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// FTypeErasedProxyFactory Implementation
	//////////////////////////////////////////////////////////////////////////
	
	TSharedPtr<FTypeErasedProxy> FTypeErasedProxyFactory::CreateFromBuffer(
		const TSharedPtr<IBuffer>& InBuffer,
		EPCGMetadataTypes InWorkingType)
	{
		if (!InBuffer)
		{
			return nullptr;
		}
		
		auto Proxy = MakeShared<FTypeErasedProxy>();
		
		Proxy->SourceType = EProxySourceType::AttributeBuffer;
		Proxy->RealType = InBuffer->GetType();
		Proxy->WorkingType = InWorkingType;
		Proxy->BufferStorage = InBuffer;
		Proxy->BufferPtr = &Proxy->BufferStorage;
		
		Proxy->InitializeTypeOps();
		
		// Get accessor functions
		ProxyAccessors::GetBufferAccessors(
			Proxy->RealType,
			Proxy->WorkingType,
			Proxy->GetFn,
			Proxy->SetFn,
			Proxy->GetCurrentFn,
			Proxy->GetHashFn);
		
		return Proxy;
	}
	
	TSharedPtr<FTypeErasedProxy> FTypeErasedProxyFactory::CreateConstant(
		EPCGMetadataTypes ValueType,
		const void* Value,
		EPCGMetadataTypes InWorkingType)
	{
		auto Proxy = MakeShared<FTypeErasedProxy>();
		
		Proxy->SourceType = EProxySourceType::Constant;
		Proxy->RealType = ValueType;
		Proxy->WorkingType = InWorkingType;
		Proxy->ConstantValuePtr = Proxy->GetMutableConstantStorage();
		
		Proxy->InitializeTypeOps();
		
		// Convert and store the constant in working type
		if (ValueType == InWorkingType)
		{
			// Direct copy
			if (Proxy->WorkingTypeOps)
			{
				Proxy->WorkingTypeOps->Copy(Value, Proxy->GetMutableConstantStorage());
			}
		}
		else
		{
			// Convert
			PCGExTypeOps::FConversionTable::Convert(ValueType, Value, InWorkingType, Proxy->GetMutableConstantStorage());
		}
		
		// Set accessor functions
		Proxy->GetFn = ProxyAccessors::GetConstantGetFn(InWorkingType);
		Proxy->SetFn = nullptr; // Cannot set constant
		Proxy->GetCurrentFn = Proxy->GetFn; // Same as Get for constant
		Proxy->GetHashFn = nullptr; // TODO: Implement constant hash
		
		return Proxy;
	}
	
	TSharedPtr<FTypeErasedProxy> FTypeErasedProxyFactory::CreateForPointProperty(
		UPCGBasePointData* InPointData,
		int32 InPropertyEnum,
		bool bIsExtraProperty,
		EPCGMetadataTypes InWorkingType)
	{
		if (!InPointData)
		{
			return nullptr;
		}
		
		auto Proxy = MakeShared<FTypeErasedProxy>();
		
		Proxy->SourceType = bIsExtraProperty ? EProxySourceType::PointExtraProperty : EProxySourceType::PointProperty;
		Proxy->WorkingType = InWorkingType;
		Proxy->PointData = InPointData;
		Proxy->PropertyEnum = InPropertyEnum;
		
		// TODO: Determine RealType from PropertyEnum
		// For now, assume same as WorkingType
		Proxy->RealType = InWorkingType;
		
		Proxy->InitializeTypeOps();
		
		// TODO: Set accessor functions for point properties
		// This requires specialized accessors per property
		
		return Proxy;
	}
	
	TSharedPtr<FTypeErasedProxy> FTypeErasedProxyFactory::CreateDirectAttribute(
		FPCGMetadataAttributeBase* InAttribute,
		FPCGMetadataAttributeBase* OutAttribute,
		UPCGBasePointData* InPointData,
		EPCGMetadataTypes InWorkingType)
	{
		if (!InAttribute && !OutAttribute)
		{
			return nullptr;
		}
		
		auto Proxy = MakeShared<FTypeErasedProxy>();
		
		Proxy->SourceType = EProxySourceType::DirectAttribute;
		Proxy->RealType = InAttribute ? static_cast<EPCGMetadataTypes>(InAttribute->GetTypeId()) : 
		                  static_cast<EPCGMetadataTypes>(OutAttribute->GetTypeId());
		Proxy->WorkingType = InWorkingType;
		Proxy->DirectAttribute = InAttribute ? InAttribute : OutAttribute;
		Proxy->PointData = InPointData;
		
		Proxy->InitializeTypeOps();
		
		// TODO: Set accessor functions for direct attribute access
		
		return Proxy;
	}
	
	TSharedPtr<FTypeErasedProxy> FTypeErasedProxyFactory::Create(
		FPCGExContext* InContext,
		const FProxyDescriptor& InDescriptor)
	{
		// This is the main entry point - dispatches based on descriptor
		// Implementation depends on full FProxyDescriptor definition
		
		if (InDescriptor.bIsConstant)
		{
			// TODO: Get constant value from descriptor
			return nullptr;
		}
		
		// TODO: Determine source type from selector and create appropriate proxy
		// This requires access to the full proxy infrastructure
		
		return nullptr;
	}
}