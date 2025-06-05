// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"
#include "PCGExPointsProcessor.h"


#include "PCGExFuseCollinear.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/fuse-collinear"))
class UPCGExFuseCollinearSettings : public UPCGExPathProcessorSettings
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
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourceKeepConditionLabel, "List of filters that are checked to know whether a point can be removed or must be kept.", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings


	/** Angular threshold for collinearity. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Units="Degrees", ClampMin=0, ClampMax=180))
	double Threshold = 10;

	/** Fuse points that are not collinear (Smooth-like). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvertThreshold = false;

	/** If enabled, will consider collocated points as collinear */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bFuseCollocated = true;

	/** Distance used to consider point to be overlapping. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ClampMin=0.001, EditCondition="bFuseCollocated"))
	double FuseDistance = 0.001;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	//bool bDoBlend = false;

	//UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties, EditCondition="bDoBlend"))
	//TObjectPtr<UPCGExSubPointsBlendOperation> Blending;

	/** Distance used to consider point to be overlapping. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bOmitInvalidPathsFromOutput = true;
};

struct FPCGExFuseCollinearContext final : FPCGExPathProcessorContext
{
	friend class FPCGExFuseCollinearElement;

	double DotThreshold = 0;
	double FuseDistSquared = 0;
	//bool bDoBlend;
	//UPCGExSubPointsBlendOperation* Blending = nullptr;
};

class FPCGExFuseCollinearElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(FuseCollinear)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExFuseCollinear
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExFuseCollinearContext, UPCGExFuseCollinearSettings>
	{
		TSharedPtr<PCGExPaths::FPath> Path;

		TArray<int32> ReadIndices;
		FVector LastPosition = FVector::ZeroVector;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
			DefaultPointFilterValue = false;
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}
