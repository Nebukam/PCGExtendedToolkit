// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Types/PCGExTypeOpsImpl.h"

#include "Helpers/PCGExMetaHelpers.h"
#include "Types/PCGExTypeOps.h"

namespace PCGExTypeOps
{
	// FTypeOpsRegistry Implementation
	TArray<TUniquePtr<ITypeOpsBase>> FTypeOpsRegistry::TypeOps;
	bool FTypeOpsRegistry::bInitialized = false;

	const ITypeOpsBase* FTypeOpsRegistry::Get(const EPCGMetadataTypes Type)
	{
#define PCGEX_TPL(_TYPE, _NAME, ...) case EPCGMetadataTypes::_NAME: return &TTypeOpsImpl<_TYPE>::GetInstance();
		switch (Type)
		{
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
		default: return nullptr;
		}
#undef PCGEX_TPL
	}

	void FTypeOpsRegistry::Initialize()
	{
		// Using static instances via GetInstance(), no additional initialization needed
		bInitialized = true;
	}

	// FConversionTable Implementation
	FConvertFn FConversionTable::Table[PCGExTypes::TypesAllocations][PCGExTypes::TypesAllocations] = {};
	bool FConversionTable::bInitialized = false;

	namespace
	{
		// Helper to populate a row of the conversion table
		template <typename TFrom>
		void PopulateConversionRow(FConvertFn* Row)
		{
			using namespace ConversionFunctions;

			int32 Idx = 0;
#define PCGEX_TPL(_TYPE, _NAME, ...) Row[Idx++] = GetConvertFunction<TFrom, _TYPE>();
			PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL
		}
	}

	void FConversionTable::Initialize()
	{
		if (bInitialized) { return; }

		int32 Idx = 0;
#define PCGEX_TPL(_TYPE, _NAME, ...) PopulateConversionRow<_TYPE>(Table[Idx++]);
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

		bInitialized = true;
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
#define PCGEX_TPL(_TYPE, _NAME, ...) \
	template class TTypeOpsImpl<_TYPE>; \
	template PCGEXCORE_API const ITypeOpsBase* FTypeOpsRegistry::Get<_TYPE>();
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL
}
