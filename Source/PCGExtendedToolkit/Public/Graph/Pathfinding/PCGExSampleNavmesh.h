// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"

#include "PCGExSampleNavmesh.generated.h"

namespace PCGExNavmesh
{
}

UENUM(BlueprintType)
enum class EPCGExPathfindingMode : uint8
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
	PCGEX_NODE_INFOS(SampleNavmesh, "Sample Navmesh", "Sample world navmesh.");
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual PCGExIO::EInitMode GetPointOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;

	virtual FName GetMainPointsInputLabel() const override;
	virtual FName GetMainPointsOutputLabel() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bAddSeedToPath = true;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bAddGoalToPath = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bRequireNaviguableEndLocation = true;
	
	/** Fuse points by distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	double FuseDistance = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExPathfindingMode PathfindingMode = EPCGExPathfindingMode::Regular;
	
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
	bool bAddSeedToPath = true;
	bool bAddGoalToPath = true;
	FNavAgentProperties NavAgentProperties;
	ANavigationData* NavData = nullptr;
	bool bRequireNaviguableEndLocation = true;
	EPCGExPathfindingMode PathfindingMode;
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
class PCGEXTENDEDTOOLKIT_API FNavmeshPathTask : public FPointTask
{
public:
	FNavmeshPathTask(FPCGExPointsProcessorContext* InContext, UPCGExPointIO* InPointData, const PCGExMT::FTaskInfos& InInfos) :
		FPointTask(InContext, InPointData, InInfos)
	{
	}

	int32 GoalIndex = -1;
	UPCGPointData* PathPoints;
	
	virtual void ExecuteTask() override;
	
};
