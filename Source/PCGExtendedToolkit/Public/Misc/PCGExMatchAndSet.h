// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"

#include "PCGExMatchAndSet.generated.h"

class UPCGExMatchAndSetFactoryBase;
/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExMatchAndSetSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(MatchAndSet, "Match And Set", "TBD.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscAdd; }
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExAttributeGatherSettings DefaultAttributesFilter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bDoConsumeProcessedAttributes = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bDoConsumeProcessedAttributes"))
	FPCGExAttributeFilterSettings ConsumeProcessedAttributes;

private:
	friend class FPCGExMatchAndSetElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExMatchAndSetContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExMatchAndSetElement;

	virtual ~FPCGExMatchAndSetContext() override;

	PCGEx::FAttributesInfos* DefaultAttributes = nullptr;
	TArray<UPCGExMatchAndSetFactoryBase*> MatchAndSetsFactories;
};

class PCGEXTENDEDTOOLKIT_API FPCGExMatchAndSetElement final : public FPCGExPointsProcessorElement
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

namespace PCGExMatchAndSet
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
