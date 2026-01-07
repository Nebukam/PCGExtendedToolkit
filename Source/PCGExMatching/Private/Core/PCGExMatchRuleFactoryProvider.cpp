// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExMatchRuleFactoryProvider.h"

#include "Data/PCGExPointIO.h"

#define LOCTEXT_NAMESPACE "PCGExCreateMatchRule"
#define PCGEX_NAMESPACE CreateMatchRule

PCG_DEFINE_TYPE_INFO(FPCGExDataTypeInfoMatchRule, UPCGExMatchRuleFactoryData)

bool FPCGExMatchRuleOperation::PrepareForMatchableSources(FPCGExContext* InContext, const TSharedPtr<TArray<FPCGExTaggedData>>& InMatchableSources)
{
	MatchableSources = InMatchableSources;
	return true;
}

TSharedPtr<FPCGExMatchRuleOperation> UPCGExMatchRuleFactoryData::CreateOperation(FPCGExContext* InContext) const
{
	return nullptr; // Create shape builder operation
}

UPCGExFactoryData* UPCGExMatchRuleFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	return Super::CreateFactory(InContext, InFactory);
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
