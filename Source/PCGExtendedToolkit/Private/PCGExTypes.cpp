// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExTypes.h"

namespace PCGExTypes
{

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

}
