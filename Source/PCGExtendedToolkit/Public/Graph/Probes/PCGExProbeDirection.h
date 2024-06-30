// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExProbeFactoryProvider.h"
#include "PCGExProbeOperation.h"

#include "PCGExProbeDirection.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExProbeDescriptorDirection : public FPCGExProbeDescriptorBase
{
	GENERATED_BODY()

	FPCGExProbeDescriptorDirection() :
		FPCGExProbeDescriptorBase()
	{
	}

	/** Max angle to search within. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0, ClampMax=360))
	double MaxAngle = 45;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFetchType DirectionSource = EPCGExFetchType::Constant;

	/** Constant direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0, EditCondition="DirectionSource==EPCGExFetchType::Constant", EditConditionHides))
	FVector DirectionConstant = FVector::ForwardVector;

	/** Attribute to read the direction from */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="DirectionSource==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector DirectionAttribute;

	/** Transform the direction with the point's */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bTransformDirection = true;
};

/**
 * 
 */
UCLASS(DisplayName = "Direction")
class PCGEXTENDEDTOOLKIT_API UPCGExProbeDirection : public UPCGExProbeOperation
{
	GENERATED_BODY()

public:
	virtual bool PrepareForPoints(const PCGExData::FPointIO* InPointIO) override;
	virtual void ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Stacks, const FVector& ST) override;

	FPCGExProbeDescriptorDirection Descriptor;

protected:
	bool bUseConstantDir = false;
	double MaxDot = 0;
	FVector Direction = FVector::ForwardVector;
	PCGExData::FCache<FVector>* DirectionCache = nullptr;
};

////

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExProbeFactoryDirection : public UPCGExProbeFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExProbeDescriptorDirection Descriptor;
	virtual UPCGExProbeOperation* CreateOperation() const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExProbeDirectionProviderSettings : public UPCGExProbeFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		ProbeDirection, "Probe : Direction", "Probe in a given direction.",
		FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

	/** Filter Descriptor.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExProbeDescriptorDirection Descriptor;


#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
