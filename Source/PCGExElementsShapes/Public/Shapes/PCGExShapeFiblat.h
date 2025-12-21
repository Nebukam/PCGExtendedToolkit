// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExShape.h"
#include "Core/PCGExShapeBuilderFactoryProvider.h"
#include "Core/PCGExShapeBuilderOperation.h"
#include "Core/PCGExShapeConfigBase.h"
#include "Details/PCGExInputShorthandsDetails.h"

#include "PCGExShapeFiblat.generated.h"

UENUM()
enum class EPCGExFibPhiConstant : uint8
{
	GoldenRatio    = 0 UMETA(DisplayName = "Golden Ratio"),
	SqRootOfTwo    = 1 UMETA(DisplayName = "Sqrt 2"),
	Irrational     = 2 UMETA(DisplayName = "Irrational"),
	SqRootOfTthree = 3 UMETA(DisplayName = "Sqrt 3"),
	Ln2            = 4 UMETA(DisplayName = "Ln2"),
	Custom         = 5 UMETA(DisplayName = "Custom"),
};

USTRUCT(BlueprintType)
struct FPCGExShapeFiblatConfig : public FPCGExShapeConfigBase
{
	GENERATED_BODY()

	FPCGExShapeFiblatConfig()
		: FPCGExShapeConfigBase()
	{
	}

	/** Phi Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExFibPhiConstant PhiConstant = EPCGExFibPhiConstant::GoldenRatio;

	/** Phi Custom Value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="PhiConstant == EPCGExFibPhiConstant::Custom", EditConditionHides))
	FPCGExInputShorthandNameDouble Phi = FPCGExInputShorthandNameDouble(FName("Phi"), 0.724592938, false);

	/** Epsilon */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Epsilon = 0;
};

namespace PCGExShapes
{
	class FFiblat : public FShape
	{
	public:
		double Radius = 1;
		double Phi = (FMath::Sqrt(5.0) - 1.0) * 0.5;
		double Epsilon = 0;

		explicit FFiblat(const PCGExData::FConstPoint& InPointRef)
			: FShape(InPointRef)
		{
		}

		FORCEINLINE FVector ComputeFibLatPoint(const int32 Index, const int32 Count) const
		{
			// Stolen from : https://observablehq.com/@meetamit/fibonacci-lattices

			// x = (i * phi) % 1
			const double X = FMath::Fmod(Index * Phi, 1.0);
			// y = i / (Count - 1)
			const double Y = (Index + Epsilon) / (Count - 1 + +2.0 * Epsilon);
			// θ = x * 2π
			const double Theta = X * 2.0 * PI;
			// φ = acos(1 - 2y)
			const double PhiAngle = FMath::Acos(1.0 - 2.0 * Y);

			// Convert spherical to Cartesian
			const double SinPhi = FMath::Sin(PhiAngle);
			const double CosPhi = FMath::Cos(PhiAngle);
			const double CosTh = FMath::Cos(Theta);
			const double SinTh = FMath::Sin(Theta);

			return FVector(CosTh * SinPhi, CosPhi, SinTh * SinPhi);
		}
	};
}

/**
 * 
 */
class FPCGExShapeFiblatBuilder : public FPCGExShapeBuilderOperation
{
public:
	FPCGExShapeFiblatConfig Config;

	virtual bool PrepareForSeeds(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InSeedDataFacade) override;
	virtual void PrepareShape(const PCGExData::FConstPoint& Seed) override;
	virtual void BuildShape(TSharedPtr<PCGExShapes::FShape> InShape, TSharedPtr<PCGExData::FFacade> InDataFacade, const PCGExData::FScope& Scope, bool bOwnsData = false) override;

protected:
	TSharedPtr<PCGExDetails::TSettingValue<double>> Phi;
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExShapeFiblatFactory : public UPCGExShapeBuilderFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExShapeFiblatConfig Config;

	virtual TSharedPtr<FPCGExShapeBuilderOperation> CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Builder|Params", meta=(PCGExNodeLibraryDoc="misc/shapes/shape-sphere"))
class UPCGExCreateShapeFiblatSettings : public UPCGExShapeBuilderFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(ShapeBuilderFiblat, "Shape : φ Sphere", "Create a Fibonacci Lattice sphere.", FName(TEXT("φ Sphere")))

#endif
	//~End UPCGSettings

	/** Shape properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExShapeFiblatConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

protected:
	virtual bool IsCacheable() const override { return true; }
};
