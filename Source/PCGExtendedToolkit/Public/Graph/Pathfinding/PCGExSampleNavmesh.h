// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "PCGExPathfinding.h"

#include "PCGExPointsProcessor.h"
#include "GoalPickers/PCGExGoalPicker.h"
#include "Graph/PCGExGraph.h"
#include "Splines/SubPoints/DataBlending/PCGExSubPointsBlendOperation.h"
#include "Splines/SubPoints/Orient/PCGExSubPointsOrientOperation.h"

#include "PCGExSampleNavmesh.generated.h"


UENUM(BlueprintType)
enum class EPCGExNavmeshPathfindingMode : uint8
{
	Regular UMETA(DisplayName = "Regular", ToolTip="TBD"),
	Hierarchical UMETA(DisplayName = "HIerarchical", ToolTip="TBD"),
};

/**
 * Use PCGExTransform to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExSampleNavmeshSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

	UPCGExSampleNavmeshSettings(const FObjectInitializer& ObjectInitializer);

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleNavmesh, "Sample Navmesh", "Extract paths from navmesh.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorPathfinding; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	virtual PCGExPointIO::EInit GetPointOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;

	virtual FName GetMainPointsInputLabel() const override;
	virtual FName GetMainPointsOutputLabel() const override;

public:
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

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bRequireNavigableEndLocation = true;

	/** Fuse points by distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	double FuseDistance = 10;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExNavmeshPathfindingMode PathfindingMode = EPCGExNavmeshPathfindingMode::Regular;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FNavAgentProperties NavAgentProperties;

	/** If left empty, will attempt to fetch the default nav data instance.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	ANavigationData* NavData = nullptr;
};


struct PCGEXTENDEDTOOLKIT_API FPCGExSampleNavmeshContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExSampleNavmeshElement;

public:
	UPCGExPointIO* GoalsPoints = nullptr;
	UPCGExPointIOGroup* OutputPaths = nullptr;

	UPCGExGoalPicker* GoalPicker;
	UPCGExSubPointsBlendOperation* Blending;

	bool bAddSeedToPath = true;
	bool bAddGoalToPath = true;

	FNavAgentProperties NavAgentProperties;

	ANavigationData* NavData = nullptr;

	bool bRequireNavigableEndLocation = true;
	EPCGExNavmeshPathfindingMode PathfindingMode;
	double FuseDistance = 10;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSampleNavmeshElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;
	virtual bool Validate(FPCGContext* InContext) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

// Define the background task class
class PCGEXTENDEDTOOLKIT_API FNavmeshPathTask : public FPCGExAsyncTask
{
public:
	FNavmeshPathTask(
		UPCGExAsyncTaskManager* InManager, const PCGExMT::FTaskInfos& InInfos, UPCGExPointIO* InPointIO,
		int32 InGoalIndex, UPCGExPointIO* InPathPoints) :
		FPCGExAsyncTask(InManager, InInfos, InPointIO),
		GoalIndex(InGoalIndex), PathPoints(InPathPoints)
	{
	}

	int32 GoalIndex = -1;
	UPCGExPointIO* PathPoints;

	virtual bool ExecuteTask() override;
};
