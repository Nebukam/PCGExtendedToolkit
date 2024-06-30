// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"

#include "PCGExTransmogBitmask.generated.h"

class UPCGExBitmaskTransmogFactoryBase;
/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExTransmogBitmaskSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(TransmogBitmask, "Bitmask Transmog", "Use a bitmask along with transmutation modules to turn a bitmask attribute into magic.");
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
	/** Used as default bitmask attribute to read from. \n Note that transmutations can read from a different bitmask.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName DefaultBitmaskAttribute = "Bitmask";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExAttributeGatherSettings DefaultAttributesFilter;

private:
	friend class FPCGExTransmogBitmaskElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExTransmogBitmaskContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExTransmogBitmaskElement;

	virtual ~FPCGExTransmogBitmaskContext() override;

	PCGEx::FAttributesInfos* DefaultAttributes = nullptr;
	TArray<UPCGExBitmaskTransmogFactoryBase*> TransmogsFactories;
};

class PCGEXTENDEDTOOLKIT_API FPCGExTransmogBitmaskElement final : public FPCGExPointsProcessorElement
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

namespace PCGExTransmogBitmask
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
