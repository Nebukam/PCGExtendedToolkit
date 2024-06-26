// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExBitflagOperation.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Transform Component Selector"))
enum class EPCGExBitflagOperation : uint8
{
	Set UMETA(DisplayName = "Set (Flags = Mask)", ToolTip="Create or replace."),
	AND UMETA(DisplayName = "AND (Flags &= Mask)", ToolTip="Add selected flags to an existing attribute."),
	OR UMETA(DisplayName = "OR (Flags |= Mask)", ToolTip="Add selected flags to an existing attribute."),
	ANDNOT UMETA(DisplayName = "AND NOT (Flags &= ~Mask)", ToolTip="Remove selected flags from an existing attribute."),
	XOR UMETA(DisplayName = "XOR (Flags ^= Mask)", ToolTip="Toggle selected flags on an existing attribute."),
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExBitflagOperationSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BitflagOperation, "Bitflag Operation", "Do a bitflag operation on an attribute.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscWrite; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Target attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName FlagAttribute;
	
	/** Target attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExBitflagOperation Operation;

	/** Type of Mask */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFetchType MaskType = EPCGExFetchType::Constant;

	/** Mask -- Must be int64. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="MaskType==EPCGExFetchType::Attribute", DisplayName="Mask", EditConditionHides))
	FName MaskAttribute;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="MaskType==EPCGExFetchType::Constant", DisplayName="Mask", EditConditionHides))
	FPCGExCompositeBitflagValue Mask;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBitflagOperationContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExBitflagOperationElement;

	virtual ~FPCGExBitflagOperationContext() override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExBitflagOperationElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExBitflagOperation
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		PCGEx::TFAttributeReader<int64>* Reader = nullptr;
		PCGEx::TFAttributeWriter<int64>* Writer = nullptr;

		int64 CompositeMask = 0;
		EPCGExBitflagOperation Op = EPCGExBitflagOperation::Set;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point) override;
		virtual void CompleteWork() override;
	};
}
