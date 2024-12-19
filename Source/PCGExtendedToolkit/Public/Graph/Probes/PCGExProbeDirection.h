// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExProbeFactoryProvider.h"
#include "PCGExProbeOperation.h"

#include "PCGExProbeDirection.generated.h"

UENUM()
enum class EPCGExProbeDirectionPriorization : uint8
{
	Dot  = 0 UMETA(DisplayName = "Best alignment", ToolTip="Favor the candidates that best align with the direction, as opposed to closest ones."),
	Dist = 1 UMETA(DisplayName = "Closest position", ToolTip="Favor the candidates that are the closest, even if they were not the best aligned."),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExProbeConfigDirection : public FPCGExProbeConfigBase
{
	GENERATED_BODY()

	FPCGExProbeConfigDirection() :
		FPCGExProbeConfigBase()
	{
	}

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bUseComponentWiseAngle = false;

	/** Max angle to search within. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bUseComponentWiseAngle", ClampMin=0, ClampMax=180))
	double MaxAngle = 45;

	/** Max angle to search within. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseComponentWiseAngle", ClampMin=0, ClampMax=180))
	FRotator MaxAngles = FRotator(45);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType DirectionInput = EPCGExInputValueType::Constant;

	/** Constant direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Direction", EditCondition="DirectionInput==EPCGExInputValueType::Constant", EditConditionHides))
	FVector DirectionConstant = FVector::ForwardVector;

	/** Attribute to read the direction from */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Direction", EditCondition="DirectionInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector DirectionAttribute;

	/** Transform the direction with the point's */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bTransformDirection = true;

	/** What matters more? */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExProbeDirectionPriorization Favor = EPCGExProbeDirectionPriorization::Dist;

	/** This probe will sample candidates after the other. Can yield different results. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bDoChainedProcessing = false;
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Direction")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExProbeDirection : public UPCGExProbeOperation
{
	GENERATED_BODY()

public:
	virtual bool RequiresChainProcessing() override;
	virtual bool PrepareForPoints(const TSharedPtr<PCGExData::FPointIO>& InPointIO) override;
	virtual void ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges) override;

	virtual void PrepareBestCandidate(const int32 Index, const FPCGPoint& Point, PCGExProbing::FBestCandidate& InBestCandidate) override;
	virtual void ProcessCandidateChained(const int32 Index, const FPCGPoint& Point, const int32 CandidateIndex, PCGExProbing::FCandidate& Candidate, PCGExProbing::FBestCandidate& InBestCandidate) override;
	virtual void ProcessBestCandidate(const int32 Index, const FPCGPoint& Point, PCGExProbing::FBestCandidate& InBestCandidate, TArray<PCGExProbing::FCandidate>& Candidates, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges) override;

	FPCGExProbeConfigDirection Config;

	virtual void Cleanup() override
	{
		DirectionCache.Reset();
		Super::Cleanup();
	}

protected:
	bool bUseConstantDir = false;
	double MinDot = 0;
	bool bUseBestDot = false;
	FVector Direction = FVector::ForwardVector;
	TSharedPtr<PCGExData::TBuffer<FVector>> DirectionCache;
};

////

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExProbeFactoryDirection : public UPCGExProbeFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExProbeConfigDirection Config;
	virtual UPCGExProbeOperation* CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExProbeDirectionProviderSettings : public UPCGExProbeFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		ProbeDirection, "Probe : Direction", "Probe in a given direction.",
		FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExProbeConfigDirection Config;


#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
