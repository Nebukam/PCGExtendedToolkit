// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/PCGExNoise3DFactoryProvider.h"
#include "Core/PCGExNoise3DOperation.h"

#include "PCGExNoiseGabor.generated.h"

USTRUCT(BlueprintType)
struct FPCGExNoiseConfigGabor : public FPCGExNoise3DConfigBase
{
	GENERATED_BODY()

	FPCGExNoiseConfigGabor() : FPCGExNoise3DConfigBase()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0001"))
	double Frequency = 1.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	int32 Seed = 1337;

	/** Direction of the gabor kernel (will be normalized) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector Direction = FVector(1, 0, 0);

	/** Bandwidth - controls how directional (lower = more directional) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.1", ClampMax = "10.0"))
	double Bandwidth = 1.0;

	/** Number of impulses per cell */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "1", ClampMax = "32"))
	int32 ImpulsesPerCell = 8;

	/** Kernel radius */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.5", ClampMax = "4.0"))
	double KernelRadius = 1.5;
};

/**
 * Gabor noise - anisotropic noise with controllable direction
 * Good for: wood grain, fabric, brushed metal, directional patterns
 */
class PCGEXNOISE3D_API FPCGExNoiseGabor : public FPCGExNoise3DOperation
{
public:
	FVector Direction = FVector(1, 0, 0);
	double Bandwidth = 1.0;
	int32 ImpulsesPerCell = 8;
	double KernelRadius = 1.5;

	virtual ~FPCGExNoiseGabor() override = default;

protected:
	virtual double GenerateRaw(const FVector& Position) const override;

private:
	FORCEINLINE double GaborKernel(const FVector& Offset, double K, double A) const;
	FORCEINLINE FVector RandomImpulse(int32 CellX, int32 CellY, int32 CellZ, int32 Idx) const;
};

////

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category = "PCGEx|Data")
class UPCGExNoise3DFactoryGabor : public UPCGExNoise3DFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExNoiseConfigGabor Config;

	virtual TSharedPtr<FPCGExNoise3DOperation> CreateOperation(FPCGExContext* InContext) const override;
	PCGEX_NOISE3D_FACTORY_BOILERPLATE
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category = "PCGEx|Noise", meta=(PCGExNodeLibraryDoc="/misc/noises/noise-gabor"))
class UPCGExNoise3DGaborProviderSettings : public UPCGExNoise3DFactoryProviderSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Noise3DGabor, "Noise : Gabor", "Gabor noise - directional/anisotropic patterns.")
#endif

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExNoiseConfigGabor Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;
};