// Copyright Timothé Lapetite 2023
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

UENUM(BlueprintType)
enum class EPCGExSubdivideMode : uint8
{
	Distance UMETA(DisplayName = "Distance", ToolTip="Number of subdivisions depends on segment' length"),
	Count UMETA(DisplayName = "Count", ToolTip="Number of subdivisions is static"),
};

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExSubdivideSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExSubdivideSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Subdivide, "Path : Subdivide", "Subdivide paths segments.");
#endif

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual PCGExData::EInit GetMainOutputInitMode() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSubdivideMode SubdivideMethod = EPCGExSubdivideMode::Distance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="SubdivideMethod==EPCGExSubdivideMode::Distance", EditConditionHides))
	double Distance = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="SubdivideMethod==EPCGExSubdivideMode::Count", EditConditionHides))
	int32 Count = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault))
	TObjectPtr<UPCGExSubPointsBlendOperation> Blending= nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bFlagSubPoints = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bFlagSubPoints"))
	FName FlagName = "IsSubPoint";
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSubdivideContext : public FPCGExPathProcessorContext
{
	friend class FPCGExSubdivideElement;

	virtual ~FPCGExSubdivideContext() override;

	UPCGExSubPointsBlendOperation* Blending = nullptr;

	EPCGExSubdivideMode SubdivideMethod;
	double Distance;
	int32 Count;
	bool bFlagSubPoints;

	FName FlagName;
	FPCGMetadataAttribute<bool>* FlagAttribute = nullptr;

	TArray<int32> Milestones;
	TArray<PCGExMath::FPathMetrics> MilestonesMetrics;
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
