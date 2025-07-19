// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Utils/PCGPointOctree.h"


#include "PCGExCollocationCount.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="misc/collocation-count"))
class UPCGExCollocationCountSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		CollocationCount, "Collocation Count", "Write the number of time a point shares its location with another.",
		CollicationNumAttributeName);
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->NodeColorMiscWrite); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** The name of the attribute to write collocation to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName CollicationNumAttributeName = "NumCollocations";

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteLinearOccurences = false;

	/** The name of the attribute to write linear occurences to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bWriteLinearOccurences"))
	FName LinearOccurencesAttributeName = "NumLinearOccurences";

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0.01))
	double Tolerance = DBL_COLLOCATION_TOLERANCE;
};

struct FPCGExCollocationCountContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExCollocationCountElement;
};

class FPCGExCollocationCountElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(CollocationCount)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExCollocationCount
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExCollocationCountContext, UPCGExCollocationCountSettings>
	{
		double NumPoints = 0;
		double ToleranceConstant = DBL_COLLOCATION_TOLERANCE;
		TSharedPtr<PCGExData::TBuffer<int32>> CollocationWriter;
		TSharedPtr<PCGExData::TBuffer<int32>> LinearOccurencesWriter;

		const PCGPointOctree::FPointOctree* Octree = nullptr;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;

		virtual void CompleteWork() override;
	};
}
