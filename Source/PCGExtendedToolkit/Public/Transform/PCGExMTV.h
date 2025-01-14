// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDetailsData.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"

#include "PCGExMTV.generated.h"

/**
 * 
 */
UCLASS(Hidden, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExMTVSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(MTV, "MTV", "Applies minimum translation vector iteratively.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorTransform; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Max number of iterations. Will exit early if no overlap is found. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=1))
	int32 MaxIterations = 50;
	
	/** Influence Settings (not implemented yet)*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExInfluenceDetails InfluenceDetails;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMTVContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExMTVElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMTVElement final : public FPCGExPointsProcessorElement
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

namespace PCGExMTV
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExMTVContext, UPCGExMTVSettings>
	{
		FPCGExInfluenceDetails InfluenceDetails;
		TArray<FIntVector3> Forces;

		FPCGExGeo2DProjectionDetails ProjectionDetails;

		int32 NumPoints = 0;
		int32 Iterations = 0;
		int8 bFoundOverlap = 0;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void OnPointsProcessingComplete() override;
		virtual void CompleteWork() override;
	};
}
