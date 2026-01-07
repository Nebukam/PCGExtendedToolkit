// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/PCGExNoise3DFactoryProvider.h"
#include "Core/PCGExNoise3DOperation.h"

#include "PCGExNoiseSpots.generated.h"

UENUM(BlueprintType)
enum class EPCGExSpotsShape : uint8
{
	Circle UMETA(DisplayName = "Circle (Hard)"),
	SoftCircle UMETA(DisplayName = "Circle (Soft Falloff)"),
	Square UMETA(DisplayName = "Square"),
	Diamond UMETA(DisplayName = "Diamond"),
	Star UMETA(DisplayName = "Star")
};

USTRUCT(BlueprintType)
struct FPCGExNoiseConfigSpots : public FPCGExNoise3DConfigBase
{
	GENERATED_BODY()

	FPCGExNoiseConfigSpots() : FPCGExNoise3DConfigBase()
	{
	}

	/** Shape of the spots */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExSpotsShape Shape = EPCGExSpotsShape::SoftCircle;

	/** Base radius of spots (0-1, relative to cell size) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.1", ClampMax = "0.8"))
	double SpotRadius = 0.4;

	/** Random variation in spot radius */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0", ClampMax = "0.5"))
	double RadiusVariation = 0.1;

	/** Jitter of spot positions within cells */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0", ClampMax = "0.5"))
	double Jitter = 0.3;

	/** Invert spots (holes instead of dots) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bInvertSpots = false;

	/** Random value variation per spot */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0", ClampMax = "1.0"))
	double ValueVariation = 0.0;
};

/**
 * Spots noise - circular/shaped spot patterns
 * Good for: polka dots, leopard spots, cell patterns, stylized textures
 */
class PCGEXNOISE3D_API FPCGExNoiseSpots : public FPCGExNoise3DOperation
{
public:
	EPCGExSpotsShape Shape = EPCGExSpotsShape::SoftCircle;
	double SpotRadius = 0.4;
	double RadiusVariation = 0.1;
	double Jitter = 0.3;
	bool bInvertSpots = false;
	double ValueVariation = 0.0;

	virtual ~FPCGExNoiseSpots() override = default;

protected:
	virtual double GenerateRaw(const FVector& Position) const override;

private:
	FVector GetSpotCenter(int32 CellX, int32 CellY, int32 CellZ) const;
	double GetSpotRadius(int32 CellX, int32 CellY, int32 CellZ) const;
	double GetSpotValue(int32 CellX, int32 CellY, int32 CellZ) const;
	double ComputeShapeDistance(const FVector& Offset, double Radius) const;
};

////

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category = "PCGEx|Data")
class UPCGExNoise3DFactorySpots : public UPCGExNoise3DFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExNoiseConfigSpots Config;

	virtual TSharedPtr<FPCGExNoise3DOperation> CreateOperation(FPCGExContext* InContext) const override;
	PCGEX_NOISE3D_FACTORY_BOILERPLATE
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category = "PCGEx|Noise", meta=(PCGExNodeLibraryDoc="/misc/noises/noise-spots"))
class UPCGExNoise3DSpotsProviderSettings : public UPCGExNoise3DFactoryProviderSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Noise3DSpots, "Noise : Spots", "Spots noise - circular/shaped spot patterns.")
#endif

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExNoiseConfigSpots Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;
};