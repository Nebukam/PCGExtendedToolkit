// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExSubSelectionOps.h"
#include "Data/PCGExSubSelectionOpsImpl.h"

namespace PCGExData
{
	//
	// FSubSelectorRegistry implementation
	//

	TArray<TUniquePtr<ISubSelectorOps>> FSubSelectorRegistry::Ops;
	bool FSubSelectorRegistry::bInitialized = false;

	void FSubSelectorRegistry::Initialize()
	{
		if (bInitialized) { return; }

		Ops.SetNum(15); // Index 0-14 for EPCGMetadataTypes enum values

		// Create instances for each supported type
		Ops[static_cast<int32>(EPCGMetadataTypes::Boolean)] = MakeUnique<TSubSelectorOpsImpl<bool>>();
		Ops[static_cast<int32>(EPCGMetadataTypes::Integer32)] = MakeUnique<TSubSelectorOpsImpl<int32>>();
		Ops[static_cast<int32>(EPCGMetadataTypes::Integer64)] = MakeUnique<TSubSelectorOpsImpl<int64>>();
		Ops[static_cast<int32>(EPCGMetadataTypes::Float)] = MakeUnique<TSubSelectorOpsImpl<float>>();
		Ops[static_cast<int32>(EPCGMetadataTypes::Double)] = MakeUnique<TSubSelectorOpsImpl<double>>();
		Ops[static_cast<int32>(EPCGMetadataTypes::Vector2)] = MakeUnique<TSubSelectorOpsImpl<FVector2D>>();
		Ops[static_cast<int32>(EPCGMetadataTypes::Vector)] = MakeUnique<TSubSelectorOpsImpl<FVector>>();
		Ops[static_cast<int32>(EPCGMetadataTypes::Vector4)] = MakeUnique<TSubSelectorOpsImpl<FVector4>>();
		Ops[static_cast<int32>(EPCGMetadataTypes::Quaternion)] = MakeUnique<TSubSelectorOpsImpl<FQuat>>();
		Ops[static_cast<int32>(EPCGMetadataTypes::Rotator)] = MakeUnique<TSubSelectorOpsImpl<FRotator>>();
		Ops[static_cast<int32>(EPCGMetadataTypes::Transform)] = MakeUnique<TSubSelectorOpsImpl<FTransform>>();
		Ops[static_cast<int32>(EPCGMetadataTypes::String)] = MakeUnique<TSubSelectorOpsImpl<FString>>();
		Ops[static_cast<int32>(EPCGMetadataTypes::Name)] = MakeUnique<TSubSelectorOpsImpl<FName>>();
		Ops[static_cast<int32>(EPCGMetadataTypes::SoftObjectPath)] = MakeUnique<TSubSelectorOpsImpl<FSoftObjectPath>>();
		Ops[static_cast<int32>(EPCGMetadataTypes::SoftClassPath)] = MakeUnique<TSubSelectorOpsImpl<FSoftClassPath>>();

		bInitialized = true;
	}

	const ISubSelectorOps* FSubSelectorRegistry::Get(EPCGMetadataTypes Type)
	{
		check(bInitialized)
		//if (!bInitialized) { Initialize(); }

		const int32 Index = static_cast<int32>(Type);
		if (Index >= 0 && Index < Ops.Num() && Ops[Index].IsValid()) { return Ops[Index].Get(); }

		return nullptr;
	}

	//
	// Explicit template instantiations
	//

#define PCGEX_INSTANTIATE_SUBSELECTOR_OPS(_TYPE, _NAME, ...) \
	template class TSubSelectorOpsImpl<_TYPE>;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_INSTANTIATE_SUBSELECTOR_OPS)
#undef PCGEX_INSTANTIATE_SUBSELECTOR_OPS
}
