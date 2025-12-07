// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExTypeErasedProxyHelpers.h"

#include "PCGExtendedToolkit.h"
#include "Data/PCGExData.h"
#include "Data/PCGExProxyData.h"

namespace PCGExData
{
	//////////////////////////////////////////////////////////////////////////
	// ProxyHelpers Implementation
	//////////////////////////////////////////////////////////////////////////

	namespace ProxyHelpers
	{
		TSharedPtr<FTypeErasedProxy> GetProxy(
			FPCGExContext* InContext,
			const FProxyDescriptor& InDescriptor)
		{
			// Delegate to factory with full descriptor info
			return FTypeErasedProxyFactory::Create(InContext, InDescriptor);
		}

		TSharedPtr<FTypeErasedProxy> GetProxyFromBuffer(
			const TSharedPtr<IBuffer>& InBuffer,
			EPCGMetadataTypes WorkingType)
		{
			return FTypeErasedProxyFactory::CreateFromBuffer(InBuffer, WorkingType);
		}

		TSharedPtr<FTypeErasedProxy> GetConstantProxy(
			EPCGMetadataTypes ValueType,
			const void* Value,
			EPCGMetadataTypes WorkingType)
		{
			return FTypeErasedProxyFactory::CreateConstant(ValueType, Value, WorkingType);
		}

		bool GetPerFieldProxies(
			FPCGExContext* InContext,
			const FProxyDescriptor& InBaseDescriptor,
			int32 NumDesiredFields,
			TArray<TSharedPtr<FTypeErasedProxy>>& OutProxies)
		{
			OutProxies.Reset();
			OutProxies.Reserve(NumDesiredFields);

			// Create a modified descriptor for each field
			for (int32 i = 0; i < NumDesiredFields; ++i)
			{
				FProxyDescriptor FieldDescriptor = InBaseDescriptor;
				FieldDescriptor.SetFieldIndex(i);

				TSharedPtr<FTypeErasedProxy> Proxy = GetProxy(InContext, FieldDescriptor);
				if (!Proxy)
				{
					OutProxies.Reset();
					return false;
				}

				// Set sub-selection on the proxy
				Proxy->SetSubSelection(i);
				OutProxies.Add(Proxy);
			}

			return true;
		}

		TSharedPtr<IBuffer> TryGetBuffer(
			FPCGExContext* InContext,
			const FProxyDescriptor& InDescriptor,
			const TSharedPtr<FFacade>& InDataFacade)
		{
			if (!InDataFacade)
			{
				return nullptr;
			}

			// Dispatch based on real type
			const EPCGMetadataTypes RealType = InDescriptor.RealType;
			
			// This is where we'd normally call TryGetBuffer<T>, but we use runtime dispatch
			// The actual buffer retrieval depends on the selector type in the descriptor
			
			// For attribute-based access:
			const FPCGAttributePropertyInputSelector& Selector = InDescriptor.Selector;
			
			if (Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
			{
				// Get buffer from facade based on type
				switch (RealType)
				{
				case EPCGMetadataTypes::Boolean:
					return InDataFacade->GetReadable<bool>(Selector.GetName());
				case EPCGMetadataTypes::Integer32:
					return InDataFacade->GetReadable<int32>(Selector.GetName());
				case EPCGMetadataTypes::Integer64:
					return InDataFacade->GetReadable<int64>(Selector.GetName());
				case EPCGMetadataTypes::Float:
					return InDataFacade->GetReadable<float>(Selector.GetName());
				case EPCGMetadataTypes::Double:
					return InDataFacade->GetReadable<double>(Selector.GetName());
				case EPCGMetadataTypes::Vector2:
					return InDataFacade->GetReadable<FVector2D>(Selector.GetName());
				case EPCGMetadataTypes::Vector:
					return InDataFacade->GetReadable<FVector>(Selector.GetName());
				case EPCGMetadataTypes::Vector4:
					return InDataFacade->GetReadable<FVector4>(Selector.GetName());
				case EPCGMetadataTypes::Quaternion:
					return InDataFacade->GetReadable<FQuat>(Selector.GetName());
				case EPCGMetadataTypes::Rotator:
					return InDataFacade->GetReadable<FRotator>(Selector.GetName());
				case EPCGMetadataTypes::Transform:
					return InDataFacade->GetReadable<FTransform>(Selector.GetName());
				case EPCGMetadataTypes::String:
					return InDataFacade->GetReadable<FString>(Selector.GetName());
				case EPCGMetadataTypes::Name:
					return InDataFacade->GetReadable<FName>(Selector.GetName());
				case EPCGMetadataTypes::SoftObjectPath:
					return InDataFacade->GetReadable<FSoftObjectPath>(Selector.GetName());
				case EPCGMetadataTypes::SoftClassPath:
					return InDataFacade->GetReadable<FSoftClassPath>(Selector.GetName());
				default:
					return nullptr;
				}
			}

			// For point property access, we don't have a buffer
			return nullptr;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// FTypeErasedBufferHelper Implementation
	//////////////////////////////////////////////////////////////////////////

	FTypeErasedBufferHelper::FTypeErasedBufferHelper(const TSharedRef<FFacade>& InDataFacade, EMode InMode)
		: DataFacade(InDataFacade)
		, Mode(InMode)
	{
	}

	TSharedPtr<IBuffer> FTypeErasedBufferHelper::TryGetBuffer(FName InName, EPCGMetadataTypes ExpectedType)
	{
		FReadScopeLock ReadScopeLock(BufferLock);
		
		if (TSharedPtr<IBuffer>* BufferPtr = BufferMap.Find(InName))
		{
			if ((*BufferPtr)->GetType() != ExpectedType)
			{
				return nullptr;
			}
			return *BufferPtr;
		}

		return nullptr;
	}

	TSharedPtr<IBuffer> FTypeErasedBufferHelper::GetBuffer(FName InName, EPCGMetadataTypes Type)
	{
		return GetOrCreateBuffer(InName, Type, nullptr);
	}

	TSharedPtr<IBuffer> FTypeErasedBufferHelper::GetBuffer(FName InName, EPCGMetadataTypes Type, const void* DefaultValue)
	{
		return GetOrCreateBuffer(InName, Type, DefaultValue);
	}

	TSharedPtr<IBuffer> FTypeErasedBufferHelper::GetOrCreateBuffer(FName InName, EPCGMetadataTypes Type, const void* DefaultValue)
	{
		// Check if already exists
		{
			FReadScopeLock ReadScopeLock(BufferLock);
			if (TSharedPtr<IBuffer>* BufferPtr = BufferMap.Find(InName))
			{
				if ((*BufferPtr)->GetType() != Type)
				{
					UE_LOG(LogPCGEx, Error, TEXT("Attempted to get buffer with different type (%s)"), *InName.ToString());
					return nullptr;
				}
				return *BufferPtr;
			}
		}

		// Create new buffer
		{
			FWriteScopeLock WriteScopeLock(BufferLock);

			// Double-check after acquiring write lock
			if (TSharedPtr<IBuffer>* BufferPtr = BufferMap.Find(InName))
			{
				return *BufferPtr;
			}

			if (PCGEx::IsPCGExAttribute(InName))
			{
				UE_LOG(LogPCGEx, Error, TEXT("Attempted to create attribute with protected prefix (%s)"), *InName.ToString());
				return nullptr;
			}

			TSharedPtr<IBuffer> NewBuffer;

			// Create based on mode and type
			if (Mode == EMode::Write)
			{
				// Get writable buffer - dispatch by type
				switch (Type)
				{
				case EPCGMetadataTypes::Boolean:
					NewBuffer = DataFacade->GetWritable<bool>(InName, EBufferInit::Inherit);
					break;
				case EPCGMetadataTypes::Integer32:
					NewBuffer = DataFacade->GetWritable<int32>(InName, EBufferInit::Inherit);
					break;
				case EPCGMetadataTypes::Integer64:
					NewBuffer = DataFacade->GetWritable<int64>(InName, EBufferInit::Inherit);
					break;
				case EPCGMetadataTypes::Float:
					NewBuffer = DataFacade->GetWritable<float>(InName, EBufferInit::Inherit);
					break;
				case EPCGMetadataTypes::Double:
					NewBuffer = DataFacade->GetWritable<double>(InName, EBufferInit::Inherit);
					break;
				case EPCGMetadataTypes::Vector2:
					NewBuffer = DataFacade->GetWritable<FVector2D>(InName, EBufferInit::Inherit);
					break;
				case EPCGMetadataTypes::Vector:
					NewBuffer = DataFacade->GetWritable<FVector>(InName, EBufferInit::Inherit);
					break;
				case EPCGMetadataTypes::Vector4:
					NewBuffer = DataFacade->GetWritable<FVector4>(InName, EBufferInit::Inherit);
					break;
				case EPCGMetadataTypes::Quaternion:
					NewBuffer = DataFacade->GetWritable<FQuat>(InName, EBufferInit::Inherit);
					break;
				case EPCGMetadataTypes::Rotator:
					NewBuffer = DataFacade->GetWritable<FRotator>(InName, EBufferInit::Inherit);
					break;
				case EPCGMetadataTypes::Transform:
					NewBuffer = DataFacade->GetWritable<FTransform>(InName, EBufferInit::Inherit);
					break;
				case EPCGMetadataTypes::String:
					NewBuffer = DataFacade->GetWritable<FString>(InName, EBufferInit::Inherit);
					break;
				case EPCGMetadataTypes::Name:
					NewBuffer = DataFacade->GetWritable<FName>(InName, EBufferInit::Inherit);
					break;
				case EPCGMetadataTypes::SoftObjectPath:
					NewBuffer = DataFacade->GetWritable<FSoftObjectPath>(InName, EBufferInit::Inherit);
					break;
				case EPCGMetadataTypes::SoftClassPath:
					NewBuffer = DataFacade->GetWritable<FSoftClassPath>(InName, EBufferInit::Inherit);
					break;
				default:
					return nullptr;
				}
			}
			else // Read mode
			{
				// Get readable buffer - dispatch by type
				switch (Type)
				{
				case EPCGMetadataTypes::Boolean:
					NewBuffer = DataFacade->GetReadable<bool>(InName);
					break;
				case EPCGMetadataTypes::Integer32:
					NewBuffer = DataFacade->GetReadable<int32>(InName);
					break;
				case EPCGMetadataTypes::Integer64:
					NewBuffer = DataFacade->GetReadable<int64>(InName);
					break;
				case EPCGMetadataTypes::Float:
					NewBuffer = DataFacade->GetReadable<float>(InName);
					break;
				case EPCGMetadataTypes::Double:
					NewBuffer = DataFacade->GetReadable<double>(InName);
					break;
				case EPCGMetadataTypes::Vector2:
					NewBuffer = DataFacade->GetReadable<FVector2D>(InName);
					break;
				case EPCGMetadataTypes::Vector:
					NewBuffer = DataFacade->GetReadable<FVector>(InName);
					break;
				case EPCGMetadataTypes::Vector4:
					NewBuffer = DataFacade->GetReadable<FVector4>(InName);
					break;
				case EPCGMetadataTypes::Quaternion:
					NewBuffer = DataFacade->GetReadable<FQuat>(InName);
					break;
				case EPCGMetadataTypes::Rotator:
					NewBuffer = DataFacade->GetReadable<FRotator>(InName);
					break;
				case EPCGMetadataTypes::Transform:
					NewBuffer = DataFacade->GetReadable<FTransform>(InName);
					break;
				case EPCGMetadataTypes::String:
					NewBuffer = DataFacade->GetReadable<FString>(InName);
					break;
				case EPCGMetadataTypes::Name:
					NewBuffer = DataFacade->GetReadable<FName>(InName);
					break;
				case EPCGMetadataTypes::SoftObjectPath:
					NewBuffer = DataFacade->GetReadable<FSoftObjectPath>(InName);
					break;
				case EPCGMetadataTypes::SoftClassPath:
					NewBuffer = DataFacade->GetReadable<FSoftClassPath>(InName);
					break;
				default:
					return nullptr;
				}

				if (!NewBuffer)
				{
					UE_LOG(LogPCGEx, Error, TEXT("Readable attribute (%s) does not exist."), *InName.ToString());
					return nullptr;
				}
			}

			if (NewBuffer)
			{
				BufferMap.Add(InName, NewBuffer);
			}

			return NewBuffer;
		}
	}

	TSharedPtr<FTypeErasedProxy> FTypeErasedBufferHelper::GetProxy(FName InName, EPCGMetadataTypes RealType, EPCGMetadataTypes WorkingType)
	{
		// Check proxy cache first
		{
			FReadScopeLock ReadScopeLock(BufferLock);
			if (TSharedPtr<FTypeErasedProxy>* ProxyPtr = ProxyMap.Find(InName))
			{
				// Verify types match
				if ((*ProxyPtr)->GetRealType() == RealType && (*ProxyPtr)->GetWorkingType() == WorkingType)
				{
					return *ProxyPtr;
				}
			}
		}

		// Get or create buffer
		TSharedPtr<IBuffer> Buffer = GetBuffer(InName, RealType);
		if (!Buffer)
		{
			return nullptr;
		}

		// Create proxy
		TSharedPtr<FTypeErasedProxy> Proxy = FTypeErasedProxyFactory::CreateFromBuffer(Buffer, WorkingType);
		if (Proxy)
		{
			FWriteScopeLock WriteScopeLock(BufferLock);
			ProxyMap.Add(InName, Proxy);
		}

		return Proxy;
	}

	bool FTypeErasedBufferHelper::SetValue(FName InAttributeName, int32 InIndex, EPCGMetadataTypes Type, const void* Value)
	{
		TSharedPtr<IBuffer> Buffer = GetBuffer(InAttributeName, Type);
		if (!Buffer)
		{
			return false;
		}

		if (Mode == EMode::Write)
		{
			Buffer->SetValueRaw(InIndex, Value);
		}
		else
		{
			if (Buffer->IsWritable())
			{
				Buffer->SetValueRaw(InIndex, Value);
			}
			else
			{
				UE_LOG(LogPCGEx, Error, TEXT("Attempting to SET on readable (%s), this is not allowed."), *InAttributeName.ToString());
				return false;
			}
		}

		return true;
	}

	bool FTypeErasedBufferHelper::GetValue(FName InAttributeName, int32 InIndex, EPCGMetadataTypes Type, void* OutValue)
	{
		TSharedPtr<IBuffer> Buffer = GetBuffer(InAttributeName, Type);
		if (!Buffer)
		{
			return false;
		}

		if (Mode == EMode::Write)
		{
			// Read from output
			const void* ValuePtr = Buffer->GetValueRaw(InIndex);
			if (ValuePtr)
			{
				// Copy using type ops
				const PCGExTypeOps::ITypeOpsBase* Ops = PCGExTypeOps::FTypeOpsRegistry::Get(Type);
				if (Ops)
				{
					Ops->Copy(ValuePtr, OutValue);
				}
			}
		}
		else
		{
			// Read from input
			const void* ValuePtr = Buffer->ReadRaw(InIndex);
			if (ValuePtr)
			{
				const PCGExTypeOps::ITypeOpsBase* Ops = PCGExTypeOps::FTypeOpsRegistry::Get(Type);
				if (Ops)
				{
					Ops->Copy(ValuePtr, OutValue);
				}
			}
		}

		return true;
	}
}