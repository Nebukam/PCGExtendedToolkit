// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Shapes/PCGExShapeBuilderOperation.h"
#include "Shapes/PCGExShapeBuilderFactoryProvider.h"
#include "Shapes/PCGExShapes.h"

void UPCGExShapeBuilderOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExShapeBuilderOperation* TypedOther = Cast<UPCGExShapeBuilderOperation>(Other))
	{
	}
}

bool UPCGExShapeBuilderOperation::PrepareForSeeds(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InSeedDataFacade)
{
	SeedFacade = InSeedDataFacade;

	ResolutionConstant = BaseConfig.ResolutionConstant;

	if (BaseConfig.ResolutionInput == EPCGExInputValueType::Attribute)
	{
		ResolutionGetter = MakeShared<PCGEx::TAttributeBroadcaster<double>>();
		if (!ResolutionGetter->Prepare(BaseConfig.ResolutionAttribute, InSeedDataFacade->Source)) { return false; }
	}

	if (!BaseConfig.Fitting.Init(InContext, InSeedDataFacade)) { return false; }

	const int32 NumSeeds = SeedFacade->GetNum();
	PCGEx::InitArray(Shapes, NumSeeds);
	return true;
}
