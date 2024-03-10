// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Paths/SubPoints/PCGExSubPointsOperation.h"
#include "SubPoints/DataBlending/PCGExSubPointsBlendOperation.h"
#include "PCGExSubdivide.generated.h"

namespace PCGExSubdivide
{
	constexpr PCGExMT::AsyncState State_BlendingPoints = __COUNTER__;
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExSubdivideSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Subdivide, "Path : Subdivide", "Subdivide paths segments.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UObject interface
public:
	virtual void PostInitProperties() override;

#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Consider paths to be closed -- processing will wrap between first and last points. (TODO:Not implemented) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bClosedPath = false;

	/** Reference for computing the blending interpolation point point */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSubdivideMode SubdivideMethod = EPCGExSubdivideMode::Distance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="SubdivideMethod==EPCGExSubdivideMode::Distance", EditConditionHides, ClampMin=0.1))
	double Distance = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="SubdivideMethod==EPCGExSubdivideMode::Count", EditConditionHides, ClampMin=1))
	int32 Count = 10;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault))
	TObjectPtr<UPCGExSubPointsBlendOperation> Blending;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bFlagSubPoints = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bFlagSubPoints"))
	FName FlagName = "IsSubPoint";
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSubdivideContext : public FPCGExPathProcessorContext
{
	friend class FPCGExSubdivideElement;

	virtual ~FPCGExSubdivideContext() override;

	EPCGExSubdivideMode SubdivideMethod;
	UPCGExSubPointsBlendOperation* Blending = nullptr;

	double Distance;
	int32 Count;
	bool bFlagSubPoints;

	FName FlagName;
	FPCGMetadataAttribute<bool>* FlagAttribute = nullptr;

	TArray<int32> Milestones;
	TArray<PCGExMath::FPathMetricsSquared> MilestonesMetrics;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSubdivideElement : public FPCGExPathProcessorElement
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
