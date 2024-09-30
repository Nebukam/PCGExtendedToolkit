// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"
#include "PCGExPointsProcessor.h"


#include "PCGExFuseCollinear.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExFuseCollinearSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FuseCollinear, "Path : Fuse Collinear", "FuseCollinear paths points.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourceKeepConditionLabel, "List of filters that are checked to know whether a point can be removed or must be kept.", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings


public:
	/** Angular threshold for collinearity. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Units="Degrees", ClampMin=0, ClampMax=180))
	double Threshold = 10;

	/** Fuse points that are not collinear (Smooth-like). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvertThreshold = false;

	/** Distance used to consider point to be overlapping. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ClampMin=0.001))
	double FuseDistance = 0.01;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	//bool bDoBlend = false;

	//UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties, EditCondition="bDoBlend"))
	//TObjectPtr<UPCGExSubPointsBlendOperation> Blending;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFuseCollinearContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExFuseCollinearElement;

	double DotThreshold = 0;
	double FuseDistSquared = 0;
	//bool bDoBlend;
	//UPCGExSubPointsBlendOperation* Blending = nullptr;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFuseCollinearElement final : public FPCGExPathProcessorElement
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

namespace PCGExFuseCollinear
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExFuseCollinearContext, UPCGExFuseCollinearSettings>
	{
		bool bClosedLoop = false;

		TArray<FPCGPoint>* OutPoints = nullptr;

		FVector LastPosition = FVector::ZeroVector;
		FVector CurrentDirection = FVector::ZeroVector;

		int32 MaxIndex = 0;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void CompleteWork() override;
	};
}
