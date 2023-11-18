// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

/*
 * This is a dummy class to create new simple PCG nodes
 */

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExProject.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExProjectSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Project, "Project", "Use local attributes to Project & Transform points");
#endif

	virtual PCGEx::EIOInit GetPointOutputInitMode() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Behavior", meta=(InlineEditConditionToggle))
	bool bLerpToLocation = true;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Behavior", meta=(EditCondition="bLerpToLocation"))
	double LerpValue = 1;
	
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Behavior", meta=(EditCondition="bLerpToLocation", InlineEditConditionToggle))
	bool bLerpWithAttribute = true;
	
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Behavior", meta=(EditCondition="bLerpWithAttribute", EditConditionHides))
	FPCGExInputDescriptorWithSingleField LocalLerpToLocation;
	
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Behavior")
	bool bWriteLocationToAttribute = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Behavior")
	bool bWriteDirectionToAttribute = false;
	
	
	/** Maximum distance to check for closest surface.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision&Metrics")
	double MaxDistance = 1000;

	/** Collision channel to check against */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision&Metrics")
	TEnumAsByte<ECollisionChannel> CollisionChannel = static_cast<ECollisionChannel>(ECC_WorldDynamic);

	/** Ignore this graph' PCG content */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision&Metrics", AdvancedDisplay)
	bool bIgnoreSelf = true;

	/** StepSize can't get smaller than this*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision&Metrics", AdvancedDisplay)
	double MinStepSize = 1;

	/** Maximum number of attempts per point. Each attempt increases probing radius by (MaxDistance/NumMaxAttempts)*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision&Metrics", AdvancedDisplay)
	int32 NumMaxAttempts = 256;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExProjectContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExProjectElement;

public:
	FName OutName = NAME_None;
	FPCGMetadataAttribute<FVector>* HitLocationAttribute;
	FPCGMetadataAttribute<FVector>* HitNormalAttribute;

	int32 NumMaxAttempts = 100;
	double AttemptStepSize = 0;
	TEnumAsByte<ECollisionChannel> CollisionChannel;
	bool bIgnoreSelf = true;

	int64 NumSweepComplete = 0;

};

class PCGEXTENDEDTOOLKIT_API FPCGExProjectElement : public FPCGExPointsProcessorElementBase
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
