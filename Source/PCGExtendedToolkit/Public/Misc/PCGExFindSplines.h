// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"


#include "PCGExFindSplines.generated.h"

UCLASS(Abstract, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc") // Abstract until implemented
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExFindSplinesSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		FindSplines, "Find Splines", "Find splines component on a given list of actors.",
		ActorReferenceAttributeName);
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscWrite; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** The name of the attribute containing actor references to resolve and look into for all sort of splines-friendly data.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName ActorReferenceAttributeName = "ActorReference";
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFindSplinesContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExFindSplinesElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFindSplinesElement final : public FPCGExPointsProcessorElement
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

namespace PCGExFindSplines
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExFindSplinesContext, UPCGExFindSplinesSettings>
	{
	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}
