// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Shapes/PCGExShapeBuilderOperation.h"
#include "Shapes/PCGExShapeBuilderFactoryProvider.h"
#include "Shapes/PCGExShapes.h"

void UPCGExShapeBuilderOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	//if (const UPCGExShapeBuilderOperation* TypedOther = Cast<UPCGExShapeBuilderOperation>(Other))	{	}
}

bool UPCGExShapeBuilderOperation::PrepareForSeeds(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InSeedDataFacade)
{
	SeedFacade = InSeedDataFacade;

	Resolution = BaseConfig.GetValueSettingResolution();
	if(!Resolution->Init(InContext, InSeedDataFacade)){return false;}

	if (!BaseConfig.Fitting.Init(InContext, InSeedDataFacade)) { return false; }

	const int32 NumSeeds = SeedFacade->GetNum();
	PCGEx::InitArray(Shapes, NumSeeds);
	return true;
}

void UPCGExShapeBuilderOperation::Cleanup()
{
	Resolution.Reset();
	Shapes.Empty();
	Super::Cleanup();
}

void UPCGExShapeBuilderOperation::ValidateShape(const TSharedPtr<PCGExShapes::FShape> Shape)
{
	if (BaseConfig.bRemoveBelow && Shape->NumPoints < BaseConfig.MinPointCount) { Shape->bValid = 0; }
	if (BaseConfig.bRemoveAbove && Shape->NumPoints > BaseConfig.MaxPointCount) { Shape->bValid = 0; }
}
