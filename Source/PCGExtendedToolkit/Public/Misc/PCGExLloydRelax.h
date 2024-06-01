// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSettings.h"
#include "Geometry/PCGExGeo.h"
#include "PCGExLloydRelax.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExLloydRelaxSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(LloydRelax, "Lloyd Relax 3D", "Applies Lloyd relaxation to the input points.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorMisc; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=1))
	int32 Iterations = 5;

	/** Influence Settings*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExInfluenceSettings InfluenceSettings;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExLloydRelaxContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExLloydRelaxElement;

	virtual ~FPCGExLloydRelaxContext() override;

	PCGEx::FLocalSingleFieldGetter* InfluenceGetter = nullptr;
	TArray<FVector> ActivePositions;
};

class PCGEXTENDEDTOOLKIT_API FPCGExLloydRelaxElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExLloydRelax3Task : public FPCGExNonAbandonableTask
{
public:
	FPCGExLloydRelax3Task(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
	                      TArray<FVector>* InPositions,
	                      const FPCGExInfluenceSettings* InInfluenceSettings,
	                      const int32 InNumIterations,
	                      PCGEx::FLocalSingleFieldGetter* InInfluenceGetter = nullptr) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		ActivePositions(InPositions),
		InfluenceSettings(InInfluenceSettings),
		NumIterations(InNumIterations),
		InfluenceGetter(InInfluenceGetter)
	{
	}

	TArray<FVector>* ActivePositions = nullptr;
	const FPCGExInfluenceSettings* InfluenceSettings = nullptr;
	int32 NumIterations = 0;
	PCGEx::FLocalSingleFieldGetter* InfluenceGetter = nullptr;

	virtual bool ExecuteTask() override;
};
