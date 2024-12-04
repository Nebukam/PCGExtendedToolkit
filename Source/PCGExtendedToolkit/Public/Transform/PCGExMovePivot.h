// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExTransform.h"


#include "PCGExMovePivot.generated.h"

class FPCGExComputeIOBounds;


UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExMovePivotSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(MovePivot, "Move Pivot", "Move pivot point relative to its bounds.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorTransform; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExUVW UVW;

private:
	friend class FPCGExMovePivotElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMovePivotContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExMovePivotElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMovePivotElement final : public FPCGExPointsProcessorElement
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExMovePivot
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExMovePivotContext, UPCGExMovePivotSettings>
	{
		FPCGExUVW UVW;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount) override;
	};
}
