// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Shapes/Builders/PCGExShapeCircle.h"

#define LOCTEXT_NAMESPACE "PCGExCreateBuilderCircle"
#define PCGEX_NAMESPACE CreateBuilderCircle

void UPCGExShapeCircleBuilder::PrepareShape(const PCGExData::FPointRef& Seed)
{
	TSharedPtr<PCGExShapes::FCircle> Circle = MakeShared<PCGExShapes::FCircle>(Seed);
	Circle->ComputeFit(Config);
	Circle->Radius = Circle->Fit.GetExtent().X;

	Circle->NumPoints = Circle->Radius * Config.Resolution;
	
	Shapes[Seed.Index] = StaticCastSharedPtr<PCGExShapes::FShape>(Circle);
}

void UPCGExShapeCircleBuilder::BuildShape(TSharedPtr<PCGExShapes::FShape> InShape, TSharedPtr<PCGExData::FFacade> InDataFacade, TArrayView<FPCGPoint> PointView)
{
}

UPCGExShapeBuilderOperation* UPCGExShapeCircleFactory::CreateOperation(FPCGExContext* InContext) const
{
	UPCGExShapeCircleBuilder* NewOperation = InContext->ManagedObjects->New<UPCGExShapeCircleBuilder>();
	NewOperation->Config = Config;
	NewOperation->Config.Init();
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExCreateShapeCircleSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExShapeCircleFactory* NewFactory = InContext->ManagedObjects->New<UPCGExShapeCircleFactory>();
	NewFactory->Config = Config;
	return Super::CreateFactory(InContext, NewFactory);
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
