// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExCustomGraphProcessor.h"

#include "PCGExFindSocketStates.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExFindSocketStatesSettings : public UPCGExCustomGraphProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindSocketStates, "Custom Graph : Find Socket States", "Find & writes socket states and attributes. Basically a glorified if/else to streamline identification of user-defined conditions within a graph.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorGraph; }
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Allow overlaping states to overlap. Useful if you want to evaluate multiple conditions separately and output the same final state */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAllowStateOverlap = false;

	/** Write the name of the state to an attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bWriteStateName = true;

	/** Name of the attribute to write state name to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bWriteStateName"))
	FName StateNameAttributeName = "State";

	/** Name of the state to write if no conditions are met */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bWriteStateName"))
	FName StatelessName = "None";


	/** Write the value of the state to an attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bWriteStateValue = true;

	/** Name of the attribute to write state name to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bWriteStateValue"))
	FName StateValueAttributeName = "StateId";

	/** Name of the state to write if no conditions are met */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bWriteStateValue"))
	int32 StatelessValue = -1;

	/** If enabled, will also write each state as a boolean attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bWriteEachStateIndividually = false;

	/** Cleanup graph socket data from output points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bDeleteCustomGraphData = false;

private:
	friend class FPCGExFindSocketStatesElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFindSocketStatesContext : public FPCGExCustomGraphProcessorContext
{
	friend class FPCGExFindSocketStatesElement;

	virtual ~FPCGExFindSocketStatesContext() override;

	TArray<TObjectPtr<UPCGExSocketStateFactory>> StateDefinitions;
	PCGExDataState::TStatesManager* StatesManager = nullptr;
	TArray<int32> PointIndices;
};


class PCGEXTENDEDTOOLKIT_API FPCGExFindSocketStatesElement : public FPCGExCustomGraphProcessorElement
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
