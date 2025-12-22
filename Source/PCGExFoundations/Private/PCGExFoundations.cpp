// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExFoundations.h"

#if WITH_EDITOR
#include "Data/Registry/PCGDataTypeRegistry.h"
#include "Elements/Partition/PCGExModularPartitionByValues.h"
#include "Core/PCGExPointStates.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExFoundationsModule"

void FPCGExFoundationsModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExFoundationsModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#if WITH_EDITOR
void FPCGExFoundationsModule::RegisterDataTypeInfos(const TSharedPtr<FSlateStyleSet>& InStyle, FPCGDataTypeRegistry& InRegistry)
{
	IPCGExModuleInterface::RegisterDataTypeInfos(InStyle, InRegistry);

	PCGEX_REGISTER_DATA_TYPE(PartitionRule, PartitionRule)
	PCGEX_REGISTER_DATA_TYPE(PointState, PointState)
}
#endif

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExFoundationsModule, PCGExFoundations)
