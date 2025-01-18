// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExProbeFactoryProvider.h"
#include "PCGExProbeOperation.h"

#include "PCGExProbeAnisotropic.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExProbeConfigAnisotropic : public FPCGExProbeConfigBase
{
	GENERATED_BODY()

	FPCGExProbeConfigAnisotropic() :
		FPCGExProbeConfigBase()
	{
	}

	/** Max angle to search within. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0, ClampMax=11.25))
	double MaxAngle = 5;

	/** Transform the direction with the point's */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bTransformDirection = true;
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Anisotrophic")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExProbeAnisotropic : public UPCGExProbeOperation
{
	GENERATED_BODY()

public:
	virtual bool PrepareForPoints(const TSharedPtr<PCGExData::FPointIO>& InPointIO) override;
	virtual void ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges) override;

	FPCGExProbeConfigAnisotropic Config;

protected:
	double MinDot = 0;
	const FVector Directions[16] = {
		FVector(1, 0, 0),             // 0 degrees (East)
		FVector(0.9239, 0.3827, 0),   // 22.5 degrees
		FVector(0.7071, 0.7071, 0),   // 45 degrees (Northeast)
		FVector(0.3827, 0.9239, 0),   // 67.5 degrees
		FVector(0, 1, 0),             // 90 degrees (North)
		FVector(-0.3827, 0.9239, 0),  // 112.5 degrees
		FVector(-0.7071, 0.7071, 0),  // 135 degrees (Northwest)
		FVector(-0.9239, 0.3827, 0),  // 157.5 degrees
		FVector(-1, 0, 0),            // 180 degrees (West)
		FVector(-0.9239, -0.3827, 0), // 202.5 degrees
		FVector(-0.7071, -0.7071, 0), // 225 degrees (Southwest)
		FVector(-0.3827, -0.9239, 0), // 247.5 degrees
		FVector(0, -1, 0),            // 270 degrees (South)
		FVector(0.3827, -0.9239, 0),  // 292.5 degrees
		FVector(0.7071, -0.7071, 0),  // 315 degrees (Southeast)
		FVector(0.9239, -0.3827, 0)   // 337.5 degrees
	};
};

////

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExProbeFactoryAnisotropic : public UPCGExProbeFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExProbeConfigAnisotropic Config;

	virtual UPCGExProbeOperation* CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExProbeAnisotropicProviderSettings : public UPCGExProbeFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		ProbeAnisotropic, "Probe : Anisotropic", "Probe in 16 directions over the X/Y axis. It's recommended to use internal projection to get the best results",
		FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExProbeConfigAnisotropic Config;


#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
