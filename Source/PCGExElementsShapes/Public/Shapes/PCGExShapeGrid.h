// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExShape.h"
#include "Core/PCGExShapeBuilderFactoryProvider.h"
#include "Core/PCGExShapeBuilderOperation.h"

#include "PCGExShapeGrid.generated.h"

USTRUCT(BlueprintType)
struct FPCGExShapeGridConfig : public FPCGExShapeConfigBase
{
	GENERATED_BODY()

	FPCGExShapeGridConfig()
		: FPCGExShapeConfigBase(true)
	{
	}
	
	/** Whether to adjust the cell size for the best fit */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ResolutionMode==EPCGExResolutionMode::Distance"))
	bool bAdjustFit = true;
};

namespace PCGExShapes
{
	class FGrid : public FShape
	{
	public:
		FIntVector3 Count = FIntVector3(5);
		FVector Extents = FVector(10.0);
		FVector Offset = FVector(0);
		bool bClosedLoop = false;

		explicit FGrid(const PCGExData::FConstPoint& InPointRef)
			: FShape(InPointRef)
		{
		}
	};
}

/**
 * 
 */
class FPCGExShapeGridBuilder : public FPCGExShapeBuilderOperation
{
public:
	FPCGExShapeGridConfig Config;

	virtual bool PrepareForSeeds(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InSeedDataFacade) override;
	virtual void PrepareShape(const PCGExData::FConstPoint& Seed) override;
	virtual void BuildShape(TSharedPtr<PCGExShapes::FShape> InShape, TSharedPtr<PCGExData::FFacade> InDataFacade, const PCGExData::FScope& Scope, bool bOwnsData = false) override;

protected:
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExShapeGridFactory : public UPCGExShapeBuilderFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExShapeGridConfig Config;

	virtual TSharedPtr<FPCGExShapeBuilderOperation> CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Builder|Params", meta=(PCGExNodeLibraryDoc="misc/shapes/shape-grid"))
class UPCGExCreateShapeGridSettings : public UPCGExShapeBuilderFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(ShapeBuilderGrid, "Shape : Grid", "Create points in a 3D grid shape.", FName("Grid"))

#endif
	//~End UPCGSettings

	/** Shape properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExShapeGridConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

protected:
	virtual bool IsCacheable() const override { return true; }
};
