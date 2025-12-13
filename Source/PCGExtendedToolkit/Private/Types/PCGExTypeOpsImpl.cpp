// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Types/PCGExTypeOpsImpl.h"
#include "Types/PCGExTypeOps.h"

namespace PCGExTypeOps
{
	// FTypeOpsRegistry Implementation
	TArray<TUniquePtr<ITypeOpsBase>> FTypeOpsRegistry::TypeOps;
	bool FTypeOpsRegistry::bInitialized = false;

	const ITypeOpsBase* FTypeOpsRegistry::Get(const EPCGMetadataTypes Type)
	{
		switch (Type)
		{
		case EPCGMetadataTypes::Boolean: return &TTypeOpsImpl<bool>::GetInstance();
		case EPCGMetadataTypes::Integer32: return &TTypeOpsImpl<int32>::GetInstance();
		case EPCGMetadataTypes::Integer64: return &TTypeOpsImpl<int64>::GetInstance();
		case EPCGMetadataTypes::Float: return &TTypeOpsImpl<float>::GetInstance();
		case EPCGMetadataTypes::Double: return &TTypeOpsImpl<double>::GetInstance();
		case EPCGMetadataTypes::Vector2: return &TTypeOpsImpl<FVector2D>::GetInstance();
		case EPCGMetadataTypes::Vector: return &TTypeOpsImpl<FVector>::GetInstance();
		case EPCGMetadataTypes::Vector4: return &TTypeOpsImpl<FVector4>::GetInstance();
		case EPCGMetadataTypes::Quaternion: return &TTypeOpsImpl<FQuat>::GetInstance();
		case EPCGMetadataTypes::Rotator: return &TTypeOpsImpl<FRotator>::GetInstance();
		case EPCGMetadataTypes::Transform: return &TTypeOpsImpl<FTransform>::GetInstance();
		case EPCGMetadataTypes::String: return &TTypeOpsImpl<FString>::GetInstance();
		case EPCGMetadataTypes::Name: return &TTypeOpsImpl<FName>::GetInstance();
		case EPCGMetadataTypes::SoftObjectPath: return &TTypeOpsImpl<FSoftObjectPath>::GetInstance();
		case EPCGMetadataTypes::SoftClassPath: return &TTypeOpsImpl<FSoftClassPath>::GetInstance();
		default: return nullptr;
		}
	}

	EPCGMetadataTypes FTypeOpsRegistry::GetTypeIdFromIndex(int32 Index)
	{
		return GetTypeFromIndex(Index);
	}

	int32 FTypeOpsRegistry::GetIndexFromTypeId(EPCGMetadataTypes Type)
	{
		return GetTypeIndex(Type);
	}

	void FTypeOpsRegistry::Initialize()
	{
		// Using static instances via GetInstance(), no additional initialization needed
		bInitialized = true;
	}

	// FConversionTable Implementation
	FConvertFn FConversionTable::Table[14][14] = {};
	bool FConversionTable::bInitialized = false;

	namespace
	{
		// Helper to populate a row of the conversion table
		template <typename TFrom>
		void PopulateConversionRow(FConvertFn* Row)
		{
			using namespace ConversionFunctions;

			Row[0] = GetConvertFunction<TFrom, bool>();
			Row[1] = GetConvertFunction<TFrom, int32>();
			Row[2] = GetConvertFunction<TFrom, int64>();
			Row[3] = GetConvertFunction<TFrom, float>();
			Row[4] = GetConvertFunction<TFrom, double>();
			Row[5] = GetConvertFunction<TFrom, FVector2D>();
			Row[6] = GetConvertFunction<TFrom, FVector>();
			Row[7] = GetConvertFunction<TFrom, FVector4>();
			Row[8] = GetConvertFunction<TFrom, FQuat>();
			Row[9] = GetConvertFunction<TFrom, FRotator>();
			Row[10] = GetConvertFunction<TFrom, FTransform>();
			Row[11] = GetConvertFunction<TFrom, FString>();
			Row[12] = GetConvertFunction<TFrom, FName>();
			Row[13] = GetConvertFunction<TFrom, FSoftObjectPath>();
			// Note: SoftClassPath (index 14) uses same row as SoftObjectPath in 14x14 table
		}
	}

	void FConversionTable::Initialize()
	{
		if (bInitialized)
		{
			return;
		}

		// Populate all 14 rows
		PopulateConversionRow<bool>(Table[0]);
		PopulateConversionRow<int32>(Table[1]);
		PopulateConversionRow<int64>(Table[2]);
		PopulateConversionRow<float>(Table[3]);
		PopulateConversionRow<double>(Table[4]);
		PopulateConversionRow<FVector2D>(Table[5]);
		PopulateConversionRow<FVector>(Table[6]);
		PopulateConversionRow<FVector4>(Table[7]);
		PopulateConversionRow<FQuat>(Table[8]);
		PopulateConversionRow<FRotator>(Table[9]);
		PopulateConversionRow<FTransform>(Table[10]);
		PopulateConversionRow<FString>(Table[11]);
		PopulateConversionRow<FName>(Table[12]);
		PopulateConversionRow<FSoftObjectPath>(Table[13]);

		bInitialized = true;
	}

	void FConversionTable::Convert(
		EPCGMetadataTypes FromType, const void* FromValue,
		EPCGMetadataTypes ToType, void* ToValue)
	{
		if (!bInitialized)
		{
			Initialize();
		}

		const int32 FromIdx = GetTypeIndex(FromType);
		const int32 ToIdx = GetTypeIndex(ToType);

		// Handle SoftClassPath by mapping to SoftObjectPath index
		const int32 SafeFromIdx = (FromIdx == 14) ? 13 : FromIdx;
		const int32 SafeToIdx = (ToIdx == 14) ? 13 : ToIdx;

		if (SafeFromIdx >= 0 && SafeFromIdx < 14 && SafeToIdx >= 0 && SafeToIdx < 14)
		{
			if (FConvertFn Fn = Table[SafeFromIdx][SafeToIdx])
			{
				Fn(FromValue, ToValue);
				return;
			}
		}

		// Fallback: use type ops to set default value
		if (const ITypeOpsBase* Ops = FTypeOpsRegistry::Get(ToType))
		{
			Ops->SetDefault(ToValue);
		}
	}

	FConvertFn FConversionTable::GetConversionFn(EPCGMetadataTypes FromType, EPCGMetadataTypes ToType)
	{
		if (!bInitialized)
		{
			Initialize();
		}

		const int32 FromIdx = GetTypeIndex(FromType);
		const int32 ToIdx = GetTypeIndex(ToType);

		const int32 SafeFromIdx = (FromIdx == 14) ? 13 : FromIdx;
		const int32 SafeToIdx = (ToIdx == 14) ? 13 : ToIdx;

		if (SafeFromIdx >= 0 && SafeFromIdx < 14 && SafeToIdx >= 0 && SafeToIdx < 14)
		{
			return Table[SafeFromIdx][SafeToIdx];
		}

		return nullptr;
	}

	// Module Initialization
	struct FTypeOpsModuleInit
	{
		FTypeOpsModuleInit()
		{
			FConversionTable::Initialize();
			FTypeOpsRegistry::Initialize();
		}
	};

	// Static instance triggers initialization at module load
	static FTypeOpsModuleInit GTypeOpsModuleInit;

	// Explicit Template Instantiations
	// TTypeOpsImpl instantiations
	template class TTypeOpsImpl<bool>;
	template class TTypeOpsImpl<int32>;
	template class TTypeOpsImpl<int64>;
	template class TTypeOpsImpl<float>;
	template class TTypeOpsImpl<double>;
	template class TTypeOpsImpl<FVector2D>;
	template class TTypeOpsImpl<FVector>;
	template class TTypeOpsImpl<FVector4>;
	template class TTypeOpsImpl<FQuat>;
	template class TTypeOpsImpl<FRotator>;
	template class TTypeOpsImpl<FTransform>;
	template class TTypeOpsImpl<FString>;
	template class TTypeOpsImpl<FName>;
	template class TTypeOpsImpl<FSoftObjectPath>;
	template class TTypeOpsImpl<FSoftClassPath>;
} 
