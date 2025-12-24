// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExAssetCollectionTypes.h"

namespace PCGExAssetCollection
{
	FTypeRegistry& FTypeRegistry::Get()
	{
		static FTypeRegistry Instance;
		return Instance;
	}

	FTypeId FTypeRegistry::Register(const FTypeInfo& Info)
	{
		if (Info.Id == NAME_None)
		{
			UE_LOG(LogTemp, Warning, TEXT("PCGExAssetCollection: Cannot register type with NAME_None"));
			return NAME_None;
		}

		FWriteScopeLock Lock(RegistryLock);

		if (Types.Contains(Info.Id))
		{
			UE_LOG(LogTemp, Warning, TEXT("PCGExAssetCollection: Type '%s' already registered"), *Info.Id.ToString());
			return Info.Id; // Return existing - idempotent
		}

		Types.Add(Info.Id, Info);

		if (Info.CollectionClass.IsValid())
		{
			ClassToType.Add(Info.CollectionClass, Info.Id);
		}

		if (Info.EntryStruct)
		{
			StructToType.Add(Info.EntryStruct, Info.Id);
		}

		UE_LOG(LogTemp, Verbose, TEXT("PCGExAssetCollection: Registered type '%s'"), *Info.Id.ToString());
		return Info.Id;
	}


	void FTypeRegistry::AddPendingRegistration(TFunction<void()>&& Func)
	{
		if (IsProcessed())
		{
			// Already initialized - execute immediately (for late-loading plugins)
			Func();
		}
		else
		{
			GetPendingRegistrations().Add(MoveTemp(Func));
		}
	}

	void FTypeRegistry::ProcessPendingRegistrations()
	{
		if (IsProcessed())
		{
			return;
		}
		IsProcessed() = true;

		for (auto& Func : GetPendingRegistrations()) { Func(); }
		GetPendingRegistrations().Empty();
		GetPendingRegistrations().Shrink();
	}

	TArray<TFunction<void()>>& FTypeRegistry::GetPendingRegistrations()
	{
		static TArray<TFunction<void()>> Pending;
		return Pending;
	}

	bool& FTypeRegistry::IsProcessed()
	{
		static bool bProcessed = false;
		return bProcessed;
	}

	const FTypeInfo* FTypeRegistry::Find(FTypeId Id) const
	{
		FReadScopeLock Lock(RegistryLock);
		return Types.Find(Id);
	}

	const FTypeInfo* FTypeRegistry::FindByClass(const UClass* Class) const
	{
		if (!Class)
		{
			return nullptr;
		}

		FReadScopeLock Lock(RegistryLock);

		// Direct lookup
		if (const FTypeId* Id = ClassToType.Find(MakeWeakObjectPtr(const_cast<UClass*>(Class))))
		{
			return Types.Find(*Id);
		}

		// Check parent classes
		for (const UClass* Current = Class->GetSuperClass(); Current; Current = Current->GetSuperClass())
		{
			if (const FTypeId* Id = ClassToType.Find(MakeWeakObjectPtr(const_cast<UClass*>(Current))))
			{
				return Types.Find(*Id);
			}
		}

		return nullptr;
	}

	const FTypeInfo* FTypeRegistry::FindByEntryStruct(const UScriptStruct* Struct) const
	{
		if (!Struct)
		{
			return nullptr;
		}

		FReadScopeLock Lock(RegistryLock);

		// Direct lookup
		if (const FTypeId* Id = StructToType.Find(Struct))
		{
			return Types.Find(*Id);
		}

		// Check parent structs
		for (const UScriptStruct* Current = Cast<UScriptStruct>(Struct->GetSuperStruct());
		     Current;
		     Current = Cast<UScriptStruct>(Current->GetSuperStruct()))
		{
			if (const FTypeId* Id = StructToType.Find(Current))
			{
				return Types.Find(*Id);
			}
		}

		return nullptr;
	}

	bool FTypeRegistry::IsA(FTypeId Type, FTypeId BaseType) const
	{
		if (Type == BaseType)
		{
			return true;
		}
		if (Type == NAME_None || BaseType == NAME_None)
		{
			return false;
		}

		FReadScopeLock Lock(RegistryLock);

		// Walk up the parent chain
		FTypeId Current = Type;
		while (Current != NAME_None)
		{
			if (Current == BaseType)
			{
				return true;
			}

			const FTypeInfo* Info = Types.Find(Current);
			if (!Info)
			{
				break;
			}

			Current = Info->ParentType;
		}

		return false;
	}

	void FTypeRegistry::GetAllTypeIds(TArray<FTypeId>& OutIds) const
	{
		FReadScopeLock Lock(RegistryLock);
		Types.GetKeys(OutIds);
	}
}

FPCGExCollectionTypeSet::FPCGExCollectionTypeSet(PCGExAssetCollection::FTypeId SingleType)
{
	Types.Add(SingleType);
}

FPCGExCollectionTypeSet::FPCGExCollectionTypeSet(std::initializer_list<PCGExAssetCollection::FTypeId> InTypes)
{
	for (const auto& Type : InTypes)
	{
		Types.Add(Type);
	}
}

bool FPCGExCollectionTypeSet::ContainsOrDerives(PCGExAssetCollection::FTypeId Type) const
{
	if (Types.Contains(Type))
	{
		return true;
	}

	for (const auto& T : Types)
	{
		if (PCGExAssetCollection::FTypeRegistry::Get().IsA(T, Type))
		{
			return true;
		}
	}
	return false;
}

FPCGExCollectionTypeSet FPCGExCollectionTypeSet::operator|(const FPCGExCollectionTypeSet& Other) const
{
	FPCGExCollectionTypeSet Result = *this;
	Result.Types.Append(Other.Types);
	return Result;
}

FPCGExCollectionTypeSet FPCGExCollectionTypeSet::operator&(const FPCGExCollectionTypeSet& Other) const
{
	FPCGExCollectionTypeSet Result;
	for (const auto& Type : Types)
	{
		if (Other.Types.Contains(Type))
		{
			Result.Types.Add(Type);
		}
	}
	return Result;
}
