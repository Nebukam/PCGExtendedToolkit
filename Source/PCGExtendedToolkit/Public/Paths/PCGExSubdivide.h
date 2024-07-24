// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "PCGExDetails.h"
#include "Graph/PCGExGraph.h"
#include "Paths/SubPoints/PCGExSubPointsOperation.h"
#include "SubPoints/DataBlending/PCGExSubPointsBlendOperation.h"
#include "PCGExSubdivide.generated.h"

namespace PCGExSubdivide
{
	PCGEX_ASYNC_STATE(State_BlendingPoints)
}

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExSubdivideSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Subdivide, "Path : Subdivide", "Subdivide paths segments.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

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

struct PCGEXTENDEDTOOLKIT_API FPCGExSubdivideContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExSubdivideElement;

	virtual ~FPCGExSubdivideContext() override;

	UPCGExSubPointsBlendOperation* Blending = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSubdivideElement final : public FPCGExPathProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExSubdivide
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		FPCGMetadataAttribute<bool>* FlagAttribute = nullptr;

		TArray<int32> Milestones;
		TArray<PCGExMath::FPathMetricsSquared> MilestonesMetrics;

		UPCGExSubPointsBlendOperation* Blending = nullptr;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool IsTrivial() const override { return false; } // Force non-trivial

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		void ProcessPathPoint(const int32 FromIndex, const int32 ToIndex);
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
