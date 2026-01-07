// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/PCGExNoise3DFactoryProvider.h"
#include "Core/PCGExNoise3DOperation.h"

#include "PCGExNoiseOpenSimplex2.generated.h"

USTRUCT(BlueprintType)
struct FPCGExNoiseConfigOpenSimplex2 : public FPCGExNoise3DConfigBase
{
	GENERATED_BODY()

	FPCGExNoiseConfigOpenSimplex2()
		: FPCGExNoise3DConfigBase()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "1", ClampMax = "16"))
	int32 Octaves = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "1.0", ClampMax = "4.0"))
	double Lacunarity = 2.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0", ClampMax = "1.0"))
	double Persistence = 0.5;

};

/**
 * OpenSimplex2 noise - patent-free alternative to Simplex
 * Better visual isotropy than classic Simplex
 */
class PCGEXNOISE3D_API FPCGExNoiseOpenSimplex2 : public FPCGExNoise3DOperation
{
public:
	virtual ~FPCGExNoiseOpenSimplex2() override = default;

protected:
	virtual double GenerateRaw(const FVector& Position) const override;

private:
	FORCEINLINE double Contrib(int32 XSV, int32 YSV, int32 ZSV, double DX, double DY, double DZ) const;
};

////

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category = "PCGEx|Data")
class UPCGExNoise3DFactoryOpenSimplex2 : public UPCGExNoise3DFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExNoiseConfigOpenSimplex2 Config;

	virtual TSharedPtr<FPCGExNoise3DOperation> CreateOperation(FPCGExContext* InContext) const override;
	PCGEX_NOISE3D_FACTORY_BOILERPLATE
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category = "PCGEx|Noise", meta=(PCGExNodeLibraryDoc="/misc/noises/noise-open-simplex-2"))
class UPCGExNoise3DOpenSimplex2ProviderSettings : public UPCGExNoise3DFactoryProviderSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Noise3DOpenSimplex2, "Noise : OpenSimplex2", "OpenSimplex2 - patent-free, high quality gradient noise.")
#endif

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExNoiseConfigOpenSimplex2 Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;
};
