// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExProbeFactoryProvider.h"
#include "PCGExProbeOperation.h"

#include "PCGExProbeClosest.generated.h"

namespace PCGExProbing
{
	struct FCandidate;
}

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExProbeConfigClosest : public FPCGExProbeConfigBase
{
	GENERATED_BODY()

	FPCGExProbeConfigClosest() :
		FPCGExProbeConfigBase()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFetchType MaxConnectionsSource = EPCGExFetchType::Constant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0, EditCondition="MaxConnectionsSource==EPCGExFetchType::Constant", EditConditionHides, ClampMin=0))
	int32 MaxConnectionsConstant = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="MaxConnectionsSource==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector MaxConnectionsAttribute;

	/** Attempts to prevent connections that are roughly in the same direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bPreventStacking = true;

	/** Attempts to prevent connections that are roughly in the same direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bPreventStacking", ClampMin=0.00001))
	double StackingPreventionTolerance = 0.001;

	/** Unbounded means this probe will sample ALL points to find a match. This is uber expensive. */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bUnbounded = false;
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Closest")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExProbeClosest : public UPCGExProbeOperation
{
	GENERATED_BODY()

public:
	virtual bool RequiresDirectProcessing() override;
	virtual bool PrepareForPoints(const PCGExData::FPointIO* InPointIO) override;
	virtual void ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* ConnectedSet, const FVector& ST, TSet<uint64>* OutEdges) override;
	virtual void ProcessNode(const int32 Index, const FPCGPoint& Point, TSet<uint64>* Stacks, const FVector& ST, TSet<uint64>* OutEdges) override;

	FPCGExProbeConfigClosest Config;

	int32 MaxConnections = 1;
	PCGExData::FCache<int32>* MaxConnectionsCache = nullptr;

protected:
	FVector CWStackingTolerance = FVector::ZeroVector;
};

////

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExProbeFactoryClosest : public UPCGExProbeFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExProbeConfigClosest Config;
	virtual UPCGExProbeOperation* CreateOperation() const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExProbeClosestProviderSettings : public UPCGExProbeFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		ProbeClosest, "Probe : Closests", "Probe in a given Closest.",
		FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExProbeConfigClosest Config;


#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
