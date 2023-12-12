// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPathfinding.h"
#include "Graph/PCGExGraphProcessor.h"

#include "PCGExPathfindingProcessor.generated.h"

class UPCGExGoalPicker;
class UPCGExSubPointsOrientOperation;
class UPCGExSubPointsBlendOperation;
class UPCGExPathfindingParamsData;

/**
 * A Base node to process a set of point using GraphParams.
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExPathfindingProcessorSettings : public UPCGExGraphProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathfindingProcessorSettings, "Pathfinding Processor Settings", "TOOLTIP_TEXT");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorPathfinding; }
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual PCGExData::EInit GetPointOutputInitMode() const override;
	virtual bool GetRequiresSeeds() const;
	virtual bool GetRequiresGoals() const;

	/** Ignores candidates weighting pass and always favors the closest one.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, Instanced)
	UPCGExGoalPicker* GoalPicker;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, Instanced)
	UPCGExSubPointsBlendOperation* Blending;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bAddSeedToPath = true;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bAddGoalToPath = true;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPathfindingProcessorContext : public FPCGExGraphProcessorContext
{
	friend class UPCGExPathfindingProcessorSettings;

	virtual ~FPCGExPathfindingProcessorContext() override;

	PCGExData::FPointIO* SeedsPoints = nullptr;
	PCGExData::FPointIO* GoalsPoints = nullptr;
	PCGExData::FPointIOGroup* OutputPaths = nullptr;

	UPCGExGoalPicker* GoalPicker;
	UPCGExSubPointsBlendOperation* Blending;

	bool bAddSeedToPath = true;
	bool bAddGoalToPath = true;

protected:
	int64 GetGoalIndex(const FPCGPoint& Seed, int64 SeedIndex);
};

class PCGEXTENDEDTOOLKIT_API FPCGExPathfindingProcessorElement : public FPCGExGraphProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Validate(FPCGContext* InContext) const override;
	virtual FPCGContext* InitializeContext(
		FPCGExPointsProcessorContext* InContext,
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) const override;
};
