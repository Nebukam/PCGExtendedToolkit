// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPathfinding.h"
#include "Graph/PCGExGraphProcessor.h"

#include "PCGExPathfindingProcessor.generated.h"

class UPCGExGoalPicker;
class UPCGExSubPointsOrient;
class UPCGExSubPointsDataBlend;
class UPCGExPathfindingParamsData;

/**
 * A Base node to process a set of point using GraphParams.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
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

	virtual PCGExPointIO::EInit GetPointOutputInitMode() const override;
	virtual bool GetRequiresSeeds() const;
	virtual bool GetRequiresGoals() const;

	/** Ignores candidates weighting pass and always favors the closest one.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, Instanced)
	UPCGExGoalPicker* GoalPicker;

	/** Ignores candidates weighting pass and always favors the closest one.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, Instanced)
	UPCGExSubPointsOrient* Orientation;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, Instanced)
	UPCGExSubPointsDataBlend* Blending;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPathfindingProcessorContext : public FPCGExGraphProcessorContext
{
	friend class UPCGExPathfindingProcessorSettings;

public:
	UPCGExPointIOGroup* PathsPoints = nullptr;
	UPCGExPointIO* SeedsPoints = nullptr;
	UPCGExPointIO* GoalsPoints = nullptr;

	UPCGExGoalPicker* GoalPicker;
	UPCGExSubPointsOrient* Orientation;
	UPCGExSubPointsDataBlend* Blending;

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
	virtual void InitializeContext(
		FPCGExPointsProcessorContext* InContext,
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) const override;
};
