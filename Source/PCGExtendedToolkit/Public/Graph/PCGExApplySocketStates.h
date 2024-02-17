// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExCustomGraphProcessor.h"

#include "PCGExApplySocketStates.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExApplySocketStatesSettings : public UPCGExCustomGraphProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ApplySocketStates, "Custom Graph : Apply States", "Applies socket states and attributes. Basically a glorified if/else to streamline identification of user-defined conditions within a graph.");
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual FName GetMainOutputLabel() const override;
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Write the name of the state to an attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bWriteStateName = true;
	
	/** Name of the attribute to write state name to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bWriteStateName"))
	FName StateNameAttributeName = "State";

	/** Name of the state to write if no conditions are met */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bWriteStateName && !bMarkStatelessPoints"))
	FName StatelessName = "None";

	
	/** Write the value of the state to an attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bWriteStateValue = true;
	
	/** Name of the attribute to write state name to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bWriteStateValue"))
	FName StateValueAttributeName = "StateId";

	/** Name of the state to write if no conditions are met */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bWriteStateValue && !bMarkStatelessPoints"))
	int32 StatelessValue = -1;

	
	/** Remove stateless points from the output */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="!bMarkStatelessPoints"))
	bool bPruneStatelessPoints = true;
	
	/** Cleanup graph socket data from output points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bDeleteCustomGraphData = false;

private:
	friend class FPCGExApplySocketStatesElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExApplySocketStatesContext : public FPCGExCustomGraphProcessorContext
{
	friend class FPCGExApplySocketStatesElement;

	virtual ~FPCGExApplySocketStatesContext() override;

	TArray<TObjectPtr<UPCGExSocketStateDefinition>> StateDefinitions;
	TArray<PCGExGraph::FSingleStateMapping*> StateMappings;
	TArray<TArray<bool>*> States;
	
};


class PCGEXTENDEDTOOLKIT_API FPCGExApplySocketStatesElement : public FPCGExCustomGraphProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExEvaluatePointTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExEvaluatePointTask(
		FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO)
	{
	}

	virtual bool ExecuteTask() override;
};