// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPathfinding.h"
#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExPathfindingProcessor.generated.h"

class UPCGExGoalPicker;
class UPCGExSubPointsOrientOperation;
class UPCGExSubPointsBlendOperation;
class UPCGExPathfindingParamsData;

/**
 * A Base node to process a set of point using GraphParams.
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExPathfindingProcessorSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPathfindingProcessorSettings(const FObjectInitializer& ObjectInitializer);
	
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathfindingProcessorSettings, "Pathfinding Processor Settings", "TOOLTIP_TEXT");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorPathfinding; }
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual bool GetRequiresSeeds() const;
	virtual bool GetRequiresGoals() const;

	/** Ignores candidates weighting pass and always favors the closest one.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, Instanced, meta = (NoResetToDefault))
	TObjectPtr<UPCGExGoalPicker> GoalPicker;

	/** How to blend path points between seed & goal */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, Instanced, meta = (NoResetToDefault))
	TObjectPtr<UPCGExSubPointsBlendOperation> Blending;

	/** Add seed point at the beginning of the path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bAddSeedToPath = true;

	/** Add goal point at the beginning of the path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bAddGoalToPath = true;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPathfindingProcessorContext : public FPCGExEdgesProcessorContext
{
	friend class UPCGExPathfindingProcessorSettings;

	virtual ~FPCGExPathfindingProcessorContext() override;

	PCGExData::FPointIO* SeedsPoints = nullptr;
	PCGExData::FPointIO* GoalsPoints = nullptr;
	PCGExData::FPointIOGroup* OutputPaths = nullptr;

	UPCGExGoalPicker* GoalPicker = nullptr;
	UPCGExSubPointsBlendOperation* Blending = nullptr;

	bool bAddSeedToPath = true;
	bool bAddGoalToPath = true;

protected:
	int64 GetGoalIndex(const FPCGPoint& Seed, int64 SeedIndex);
};

class PCGEXTENDEDTOOLKIT_API FPCGExPathfindingProcessorElement : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual FPCGContext* InitializeContext(
		FPCGExPointsProcessorContext* InContext,
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) const override;
};
