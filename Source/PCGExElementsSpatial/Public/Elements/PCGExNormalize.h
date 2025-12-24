// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"


#include "Core/PCGExPointsProcessor.h"
#include "Details/PCGExSettingsMacros.h"
#include "Math/PCGExMath.h"

#include "PCGExNormalize.generated.h"

namespace PCGExData
{
	class IBufferProxy;
}

class FPCGExComputeIOBounds;


UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="transform/normalize"))
class UPCGExNormalizeSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExNormalizeSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Normalize, "Normalize", "Output normalized position against data bounds to a new vector attribute.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(Transform); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const override;
	//~End UPCGSettings

	virtual PCGExData::EIOInit GetMainDataInitializationPolicy() const override;

public:
	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::Center;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FVector Offset = FVector::ZeroVector;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FVector Tile = FVector::OneVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExIndexSafety Wrapping = EPCGExIndexSafety::Tile;

	/** Which components should be one minus'd */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExBlending.EPCGExApplySampledComponentFlags"))
	uint8 OneMinus = 0;

	/** Whether to read the transform from an attribute on the edge or a constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType TransformInput = EPCGExInputValueType::Constant;

	/** Transform applied to the position before processing  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Transform (Attr)", EditCondition="TransformInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector TransformAttribute;

	/** Transform applied to the position before processing  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Transform", ClampMin=1, EditCondition="TransformInput == EPCGExInputValueType::Constant", EditConditionHides))
	FTransform TransformConstant = FTransform::Identity;

	PCGEX_SETTING_VALUE_DECL(Transform, FTransform)

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector Output;

private:
	friend class FPCGExNormalizeElement;
};

struct FPCGExNormalizeContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExNormalizeElement;

	bool bUseUnifiedBounds = false;
	FBox UnifiedBounds = FBox(ForceInit);

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExNormalizeElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(Normalize)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExNormalize
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExNormalizeContext, UPCGExNormalizeSettings>
	{
	protected:
		FBox Box = FBox(NoInit);
		FVector Size = FVector::ZeroVector;
		bool OneMinus[3] = {false, false, false};

		TSharedPtr<PCGExDetails::TSettingValue<FTransform>> TransformBuffer;
		TSharedPtr<PCGExData::IBufferProxy> OutputBuffer;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
	};
}
