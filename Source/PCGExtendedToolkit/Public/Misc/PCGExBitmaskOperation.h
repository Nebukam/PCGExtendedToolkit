// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExBitmaskOperation.generated.h"

namespace PCGExBitmask
{
	FORCEINLINE static void Do(const EPCGExBitOp Op, int64& Flags, const int64 Mask)
	{
		switch (Op)
		{
		default: ;
		case EPCGExBitOp::Set:
			Flags = Mask;
			break;
		case EPCGExBitOp::AND:
			Flags &= Mask;
			break;
		case EPCGExBitOp::OR:
			Flags |= Mask;
			break;
		case EPCGExBitOp::NOT:
			Flags &= ~Mask;
			break;
		case EPCGExBitOp::XOR:
			Flags ^= Mask;
			break;
		}
	}

	FORCEINLINE static void Do(const EPCGExBitOp Op, int64& Flags, const TArray<FClampedBit>& Mask)
	{
		switch (Op)
		{
		default: ;
		case EPCGExBitOp::Set:
			for (const FClampedBit& Bit : Mask)
			{
				if (Bit.bValue) { Flags |= Bit.Get(); } // Set the bit
				else { Flags &= ~Bit.Get(); }           // Clear the bit
			}
			break;
		case EPCGExBitOp::AND:
			for (const FClampedBit& Bit : Mask) { Flags &= Bit.Get(); }
			break;
		case EPCGExBitOp::OR:
			for (const FClampedBit& Bit : Mask) { Flags |= Bit.Get(); }
			break;
		case EPCGExBitOp::NOT:
			for (const FClampedBit& Bit : Mask) { Flags &= ~Bit.Get(); }
			break;
		case EPCGExBitOp::XOR:
			for (const FClampedBit& Bit : Mask) { Flags ^= Bit.Get(); }
			break;
		}
	}
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExBitmaskOperationSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BitmaskOperation, "Bitmask Operation", "Do a Bitmask operation on an attribute.");
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
	EPCGExBitOp Operation;

	/** Type of Mask */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFetchType MaskType = EPCGExFetchType::Constant;

	/** Mask -- Must be int64. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="MaskType==EPCGExFetchType::Attribute", DisplayName="Mask", EditConditionHides))
	FName MaskAttribute;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="MaskType==EPCGExFetchType::Constant", DisplayName="Mask", EditConditionHides))
	FPCGExBitmask BitMask;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBitmaskOperationContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExBitmaskOperationElement;

	virtual ~FPCGExBitmaskOperationContext() override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExBitmaskOperationElement final : public FPCGExPointsProcessorElement
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

namespace PCGExBitmaskOperation
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		PCGEx::FAttributeIOBase<int64>* Reader = nullptr;
		PCGEx::TFAttributeWriter<int64>* Writer = nullptr;

		int64 Mask = 0;
		EPCGExBitOp Op = EPCGExBitOp::Set;

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
