// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExProbeDirection.h"
#include "UObject/Object.h"

#include "PCGExProbeFactoryProvider.h"
#include "PCGExProbeOperation.h"
#include "Transform/Tensors/PCGExTensor.h"
#include "Transform/Tensors/PCGExTensorHandler.h"

#include "PCGExProbeTensor.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExProbeConfigTensor : public FPCGExProbeConfigBase
{
	GENERATED_BODY()

	FPCGExProbeConfigTensor() :
		FPCGExProbeConfigBase()
	{
	}

	/** Mirror (*-1) the direction sampled from the tensors. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvertTensorDirection = false;

	/** What matters more? */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExProbeDirectionPriorization Favor = EPCGExProbeDirectionPriorization::Dot;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bUseComponentWiseAngle = false;

	/** Max angle to search within. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bUseComponentWiseAngle", ClampMin=0, ClampMax=180))
	double MaxAngle = 45;

	/** Max angle to search within. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseComponentWiseAngle", ClampMin=0, ClampMax=180))
	FRotator MaxAngles = FRotator(45);

	/** This probe will sample candidates after the other. Can yield different results. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bDoChainedProcessing = false;

	/** Tensor sampling settings. Note that these are applied on the flattened sample, e.g after & on top of individual tensors' mutations. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Tensor Sampling Settings"))
	FPCGExTensorHandlerDetails TensorHandlerDetails;
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Direction")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExProbeTensor : public UPCGExProbeOperation
{
	GENERATED_BODY()

public:
	virtual bool RequiresChainProcessing() override;
	virtual bool PrepareForPoints(const TSharedPtr<PCGExData::FPointIO>& InPointIO) override;
	virtual void ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges) override;

	virtual void PrepareBestCandidate(const int32 Index, const FPCGPoint& Point, PCGExProbing::FBestCandidate& InBestCandidate) override;
	virtual void ProcessCandidateChained(const int32 Index, const FPCGPoint& Point, const int32 CandidateIndex, PCGExProbing::FCandidate& Candidate, PCGExProbing::FBestCandidate& InBestCandidate) override;
	virtual void ProcessBestCandidate(const int32 Index, const FPCGPoint& Point, PCGExProbing::FBestCandidate& InBestCandidate, TArray<PCGExProbing::FCandidate>& Candidates, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges) override;

	FPCGExProbeConfigTensor Config;
	const TArray<TObjectPtr<const UPCGExTensorFactoryData>>* TensorFactories = nullptr;
	TSharedPtr<PCGExTensor::FTensorsHandler> TensorsHandler;

	virtual void Cleanup() override
	{
		DirectionCache.Reset();
		Super::Cleanup();
	}

protected:
	bool bUseConstantDir = false;
	double Mirror = 1;
	double MinDot = 0;
	bool bUseBestDot = false;
	FVector Direction = FVector::ForwardVector;
	TSharedPtr<PCGExData::TBuffer<FVector>> DirectionCache;
};

////

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExProbeFactoryTensor : public UPCGExProbeFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExProbeConfigTensor Config;

	UPROPERTY()
	TArray<TObjectPtr<const UPCGExTensorFactoryData>> TensorFactories;
	
	virtual UPCGExProbeOperation* CreateOperation(FPCGExContext* InContext) const override;

	virtual bool GetRequiresPreparation(FPCGExContext* InContext) override { return true; }
	virtual bool Prepare(FPCGExContext* InContext) override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExProbeTensorProviderSettings : public UPCGExProbeFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		ProbeTensor, "Probe : Tensor", "Sample a tensor at point location and probe in that direction.",
		FName(GetDisplayName()))
#endif
	//~End UPCGSettings

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

public:
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExProbeConfigTensor Config;


#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

protected:
	virtual bool IsCacheable() const override { return false; }
};
