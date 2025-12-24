// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/FloodFill/FillControls/PCGExFillControlsFactoryProvider.h"


#define LOCTEXT_NAMESPACE "PCGExCreateFillControls"
#define PCGEX_NAMESPACE CreateFillControls

PCG_DEFINE_TYPE_INFO(FPCGExDataTypeInfoFillControl, UPCGExFillControlsFactoryData)

void FPCGExFillControlConfigBase::Init()
{
}

bool UPCGExFillControlsFactoryData::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	//FName Consumable = NAME_None;
	//PCGEX_CONSUMABLE_CONDITIONAL(ConfigBase.bUseLocalWeightMultiplier, ConfigBase.WeightMultiplierAttribute, Consumable)

	return true;
}

TSharedPtr<FPCGExFillControlOperation> UPCGExFillControlsFactoryData::CreateOperation(FPCGExContext* InContext) const
{
	return nullptr; // Create FillControl operation
}

UPCGExFactoryData* UPCGExFillControlsFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	return InFactory;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
