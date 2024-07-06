// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"

#include "PCGExMatchmaking.generated.h"

class UPCGExMatchToFactoryBase;
/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExMatchmakingSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Matchmaking, "Matchmaking", "TBD.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscAdd; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;
	//~End UPCGExPointsProcessorSettings

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExAttributeGatherDetails DefaultAttributesFilter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bDoConsumeProcessedAttributes = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bDoConsumeProcessedAttributes"))
	FPCGExNameFiltersDetails ConsumeProcessedAttributes;

private:
	friend class FPCGExMatchmakingElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExMatchmakingContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExMatchmakingElement;

	virtual ~FPCGExMatchmakingContext() override;

	PCGEx::FAttributesInfos* DefaultAttributes = nullptr;
	TArray<UPCGExMatchToFactoryBase*> MatchmakingsFactories;
};

class PCGEXTENDEDTOOLKIT_API FPCGExMatchmakingElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExMatchmaking
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
	public:
		FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
