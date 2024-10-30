// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExOperation.h"
#include "PCGExShapes.h"








#include "PCGExShapeBuilderOperation.generated.h"

/**
 * 
 */
UCLASS(Abstract)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExShapeBuilderOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;
	virtual bool PrepareForSeeds(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InSeedDataFacade);

	virtual void Cleanup() override
	{
		Shapes.Empty();
		Super::Cleanup();
	}

	TArray<TSharedPtr<PCGExShapes::FShape>> Shapes;
	FTransform Transform;

	virtual void PrepareShape(const PCGExData::FPointRef& Seed) { Shapes[Seed.Index] = MakeShared<PCGExShapes::FShape>(Seed); }

	virtual void BuildShape(const TSharedPtr<PCGExShapes::FShape> InShape, TSharedPtr<PCGExData::FFacade> InDataFacade, const TArrayView<FPCGPoint> PointView)
	{
	}

protected:
	TSharedPtr<PCGExData::FFacade> SeedFacade;

};
