// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "PCGExSampleNavmesh.generated.h"

class UPCGExSubPointsBlendOperation;
class UPCGExGoalPicker;

namespace PCGExSampleNavmesh
{
	const PCGExMT::AsyncState State_Pathfinding = PCGExMT::AsyncStateCounter::Unique();
	const PCGExMT::AsyncState State_WaitingPathfinding = PCGExMT::AsyncStateCounter::Unique();

	struct PCGEXTENDEDTOOLKIT_API FPath
	{
		FPath(const int32 InSeedIndex, const FVector& InStart, const int32 InGoalIndex, const FVector& InEnd):
			SeedIndex(InSeedIndex), Start(InStart), GoalIndex(InGoalIndex), End(InEnd)
		{
			Positions.Empty();
		}

		~FPath()
		{
			Positions.Empty();
		}

		TArray<FVector> Positions;
		PCGExMath::FPathMetrics Metrics;
		PCGExData::FPointIO* PathPoints = nullptr;

		int32 SeedIndex = -1;
		FVector Start;
		int32 GoalIndex = -1;
		FVector End;
	};
}

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
	virtual PCGExData::EInit GetPointOutputInitMode() const override;
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

	mutable FRWLock BufferLock;

	virtual ~FPCGExSampleNavmeshContext() override;
	
	PCGExData::FPointIO* GoalsPoints = nullptr;
	PCGExData::FPointIOGroup* OutputPaths = nullptr;

	UPCGExGoalPicker* GoalPicker = nullptr;
	UPCGExSubPointsBlendOperation* Blending = nullptr;

	bool bAddSeedToPath = true;
	bool bAddGoalToPath = true;

	TArray<PCGExSampleNavmesh::FPath> PathBuffer;

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
class PCGEXTENDEDTOOLKIT_API FNavmeshPathTask : public FPCGExNonAbandonableTask
{
public:
	FNavmeshPathTask(
		FPCGExAsyncManager* InManager, const PCGExMT::FTaskInfos& InInfos, PCGExData::FPointIO* InPointIO,
		PCGExSampleNavmesh::FPath* InPath) :
		FPCGExNonAbandonableTask(InManager, InInfos, InPointIO),
		Path(InPath)
	{
	}

	PCGExSampleNavmesh::FPath* Path = nullptr;

	virtual bool ExecuteTask() override;
};
