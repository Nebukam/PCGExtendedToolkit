// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/Bitmasks/PCGExBitmaskDetails.h"
#include "Core/PCGExPointsProcessor.h"
#include "Details/PCGExSettingsMacros.h"

#include "PCGExBitwiseOperation.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="metadata/bitmasks/bitmask-operation"))
class UPCGExBitwiseOperationSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BitwiseOperation, "Bitmask Operation", "Do a Bitmask operation on an attribute.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_OPTIN_NAME(MiscWrite); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	virtual PCGExData::EIOInit GetMainDataInitializationPolicy() const override;

public:
	/** Target attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName FlagAttribute;

	/** Target attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExBitOp Operation;

	/** Type of Mask */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType MaskInput = EPCGExInputValueType::Constant;

	/** Mask -- Must be int64. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Bitmask (Attr)", EditCondition="MaskInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName MaskAttribute;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Bitmask", EditCondition="MaskInput == EPCGExInputValueType::Constant", EditConditionHides))
	int64 Bitmask;

	PCGEX_SETTING_VALUE_DECL(Mask, int64)
};

struct FPCGExBitwiseOperationContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExBitwiseOperationElement;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExBitwiseOperationElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(BitwiseOperation)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExBitwiseOperation
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExBitwiseOperationContext, UPCGExBitwiseOperationSettings>
	{
		TSharedPtr<PCGExDetails::TSettingValue<int64>> Mask;
		TSharedPtr<PCGExData::TBuffer<int64>> Writer;

		EPCGExBitOp Op = EPCGExBitOp::Set;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}
