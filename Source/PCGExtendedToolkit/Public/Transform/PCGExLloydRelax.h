// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDetailsData.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"


#include "PCGExLloydRelax.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class UPCGExLloydRelaxSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(LloydRelax, "Lloyd Relax 3D", "Applies Lloyd relaxation to the input points.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorTransform; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=1))
	int32 Iterations = 5;

	/** Influence Settings*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExInfluenceDetails InfluenceDetails;
};

struct FPCGExLloydRelaxContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExLloydRelaxElement;
};

class FPCGExLloydRelaxElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(LloydRelax)
	
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExLloydRelax
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExLloydRelaxContext, UPCGExLloydRelaxSettings>
	{
		friend class FLloydRelaxTask;

		FPCGExInfluenceDetails InfluenceDetails;
		TArray<FVector> ActivePositions;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};

	class FLloydRelaxTask final : public PCGExMT::FPCGExIndexedTask
	{
	public:
		FLloydRelaxTask(const int32 InTaskIndex,
		                const TSharedPtr<FProcessor>& InProcessor,
		                const FPCGExInfluenceDetails* InInfluenceSettings,
		                const int32 InNumIterations) :
			FPCGExIndexedTask(InTaskIndex),
			Processor(InProcessor),
			InfluenceSettings(InInfluenceSettings),
			NumIterations(InNumIterations)
		{
		}

		TSharedPtr<FProcessor> Processor;
		const FPCGExInfluenceDetails* InfluenceSettings = nullptr;
		int32 NumIterations = 0;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};
}
