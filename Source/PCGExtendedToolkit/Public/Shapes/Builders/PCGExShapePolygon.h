// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDetailsData.h"
#include "PCGExPointsProcessor.h"

#include "Shapes/PCGExShapeBuilderFactoryProvider.h"
#include "Shapes/PCGExShapeBuilderOperation.h"

#include "PCGExShapePolygon.generated.h"

// Not sure if this should be the same class or a different one...
UENUM(BlueprintType) enum class EPCGExPolygonShapeType : uint8 {
	Convex = 0 UMETA(DisplayName = "Polygon"),
	Star = 1 UMETA(DisplayName = "Star")
};

USTRUCT(BlueprintType)
struct FPCGExShapePolygonConfig : public FPCGExShapeConfigBase {
	GENERATED_BODY();

	FPCGExShapePolygonConfig() : FPCGExShapeConfigBase() {}

	// Type of polygon we're creating
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(PCG_NotOverridable))
	EPCGExPolygonShapeType PolygonType = EPCGExPolygonShapeType::Convex;

	/*
	 * Number of vertices
	 */

	// Source
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExInputValueType NumVerticesInput = EPCGExInputValueType::Constant;

	// Attribute
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Number of Vertices (Attr)", EditCondition="NumVerticesInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector NumVerticesAttribute;
	
	// Constant
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(PCG_Overridable, DisplayName="Number of Vertices"))
	int32 NumVerticesConstant = 5;

	PCGEX_SETTING_VALUE_GET(NumVertices, int32, NumVerticesInput, NumVerticesAttribute, NumVerticesConstant)
	
	/*
	 * Skeleton
	 */

	// Source
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType AddSkeletonInput = EPCGExInputValueType::Constant;

	// Attribute
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Add Skeleton (Attr)", EditCondition="AddSkeletonInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector AddSkeletonAttribute;
	
	// Constant
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(PCG_Overridable))
	bool bAddSkeleton = false;

	PCGEX_SETTING_VALUE_GET(AddSkeleton, bool, AddSkeletonInput, AddSkeletonAttribute, bAddSkeleton)


	// Closed loop
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(PCG_Overridable))
	bool bIsClosedLoop = false;

	// Inner radius (todo)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(PCG_Overridable))
	double InnerRadius = 1.0;

	
};

namespace PCGExShapes {
	class FPolygon : public FShape {
	public:
		double Radius = 1.0;
		double InnerRadius = 1.0;
		int32 NumVertices = 5;
		int32 PointsPerEdge = 2;
		
		double EdgeLength = .2; // i.e. 1.0 / 5.0 
		bool bClosedLoop = false;
		bool bHasSkeleton = false;
		

		explicit FPolygon(const PCGExData::FConstPoint& InPointRef) : FShape(InPointRef) {}
	};
}

class FPCGExShapePolygonBuilder : public FPCGExShapeBuilderOperation {
public:
	FPCGExShapePolygonConfig Config;

	virtual bool PrepareForSeeds(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InSeedDataFacade) override;
	virtual void PrepareShape(const PCGExData::FConstPoint& Seed) override;
	virtual void BuildShape(TSharedPtr<PCGExShapes::FShape> InShape, TSharedPtr<PCGExData::FFacade> InDataFacade, const PCGExData::FScope& Scope) override;

protected:
	TSharedPtr<PCGExDetails::TSettingValue<int32>> NumVertices;
	TSharedPtr<PCGExDetails::TSettingValue<bool>> HasSkeleton;
	
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExShapePolygonFactory : public UPCGExShapeBuilderFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExShapePolygonConfig Config;

	virtual TSharedPtr<FPCGExShapeBuilderOperation> CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Builder|Params", meta=(PCGExNodeLibraryDoc="misc/shapes/shape-polygon"))
class UPCGExCreateShapePolygonSettings : public UPCGExShapeBuilderFactoryProviderSettings {
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(ShapeBuilderCircle, "Shape : Polygon", "Create points as a regular polygon or star.", FName("Polygon"))

#endif
	//~End UPCGSettings

	/** Shape properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExShapePolygonConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

protected:
	virtual bool IsCacheable() const override { return true; }
};