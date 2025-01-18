// Copyright 2024 Timothé Lapetite and contributors
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
	EPCGExInputValueType MaxConnectionsInput = EPCGExInputValueType::Constant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Max Connections", ClampMin=0, EditCondition="MaxConnectionsInput==EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0))
	int32 MaxConnectionsConstant = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Max Connections", EditCondition="MaxConnectionsInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector MaxConnectionsAttribute;

	/** Attempts to prevent connections that are roughly in the same direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bPreventCoincidence = true;

	/** Attempts to prevent connections that are roughly in the same direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bPreventCoincidence", ClampMin=0.00001))
	double CoincidencePreventionTolerance = 0.001;

};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Closest")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExProbeClosest : public UPCGExProbeOperation
{
	GENERATED_BODY()

public:
	virtual bool PrepareForPoints(const TSharedPtr<PCGExData::FPointIO>& InPointIO) override;
	virtual void ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges) override;
	virtual void ProcessNode(const int32 Index, const FPCGPoint& Point, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, const TArray<int8>& AcceptConnections) override;

	FPCGExProbeConfigClosest Config;

	int32 MaxConnections = 1;
	TSharedPtr<PCGExData::TBuffer<int32>> MaxConnectionsCache;

protected:
	FVector CWCoincidenceTolerance = FVector::ZeroVector;
};

////

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExProbeFactoryClosest : public UPCGExProbeFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExProbeConfigClosest Config;
	
	virtual UPCGExProbeOperation* CreateOperation(FPCGExContext* InContext) const override;
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

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExProbeConfigClosest Config;


#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
