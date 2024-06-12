// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "Geometry/PCGExGeo.h"
#include "PCGExShrinkPath.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Path Shrink Mode"))
enum class EPCGExPathShrinkMode : uint8
{
	Count UMETA(DisplayName = "Count", ToolTip="TBD"),
	Distance UMETA(DisplayName = "Distance", ToolTip="TBD."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Path Shrink Distance Cut Type"))
enum class EPCGExPathShrinkDistanceCutType : uint8
{
	NewPoint UMETA(DisplayName = "New Point", ToolTip="TBD"),
	Previous UMETA(DisplayName = "Previous (Ceil)", ToolTip="TBD."),
	Next UMETA(DisplayName = "Next (Floor)", ToolTip="TBD."),
	Closest UMETA(DisplayName = "Closest (Round)", ToolTip="TBD."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Path Shrink Endpoint"))
enum class EPCGExShrinkEndpoint : uint8
{
	Both UMETA(DisplayName = "Start and End", ToolTip="TBD"),
	Start UMETA(DisplayName = "Start", ToolTip="TBD."),
	End UMETA(DisplayName = "End", ToolTip="TBD."),
};

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExShrinkPathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ShrinkPath, "Path : Shrink", "Shrink path from its beginning and end.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

	virtual FName GetPointFilterLabel() const override;
	
public:
	/** Consider paths to be closed -- processing will wrap between first and last points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bClosedPath = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExPathShrinkMode ShrinkMode = EPCGExPathShrinkMode::Distance;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExFetchType ValueSource = EPCGExFetchType::Constant;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ShrinkMode==EPCGExPathShrinkMode::Count && ValueSource==EPCGExFetchType::Constant", EditConditionHides, ClampMin=1))
	int32 CountConstant = 1;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ShrinkMode==EPCGExPathShrinkMode::Distance && ValueSource==EPCGExFetchType::Constant", EditConditionHides))
	double DistanceConstant = 10;

	/** Distance or count */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ValueSource==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector ShrinkAmount;
	
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ValueSource==EPCGExFetchType::Constant", EditConditionHides))
	EPCGExShrinkEndpoint ShrinkEndpoint = EPCGExShrinkEndpoint::Both;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ShrinkMode==EPCGExPathShrinkMode::Distance", EditConditionHides))
	EPCGExPathShrinkDistanceCutType CutType = EPCGExPathShrinkDistanceCutType::NewPoint;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bEndpointsIgnoreStopConditions = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ShrinkEndpoint==EPCGExShrinkEndpoint::Both", EditConditionHides))
	EPCGExShrinkEndpoint ShrinkFirst = EPCGExShrinkEndpoint::Both;
	
};

struct PCGEXTENDEDTOOLKIT_API FPCGExShrinkPathContext : public FPCGExPathProcessorContext
{
	friend class FPCGExShrinkPathElement;

	virtual bool DefaultPointFilterResult() const override;
	virtual bool PrepareFiltersWithAdvance() const override;

	virtual ~FPCGExShrinkPathContext() override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExShrinkPathElement : public FPCGExPathProcessorElement
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

class PCGEXTENDEDTOOLKIT_API FPCGExShrinkPathTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExShrinkPathTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO)
	{
	}

	virtual bool ExecuteTask() override;
};
