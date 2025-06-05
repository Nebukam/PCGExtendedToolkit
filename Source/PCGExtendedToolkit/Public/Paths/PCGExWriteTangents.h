// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"


#include "Tangents/PCGExTangentsInstancedFactory.h"
#include "PCGExWriteTangents.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/write-tangents"))
class UPCGExWriteTangentsSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

	UPCGExWriteTangentsSettings(const FObjectInitializer& ObjectInitializer);

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathWriteTangents, "Path : Write Tangents", "Computes & writes points tangents.");
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	virtual FName GetPointFilterPin() const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName ArriveName = "ArriveTangent";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName LeaveName = "LeaveTangent";

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault))
	TObjectPtr<UPCGExTangentsInstancedFactory> Tangents;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault))
	TObjectPtr<UPCGExTangentsInstancedFactory> StartTangents;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault))
	TObjectPtr<UPCGExTangentsInstancedFactory> EndTangents;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scaling", meta=(PCG_NotOverridable))
	EPCGExInputValueType ArriveScaleInput = EPCGExInputValueType::Constant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scaling", meta=(PCG_Overridable, DisplayName="Arrive Scale (Attr)", EditCondition="ArriveScaleInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector ArriveScaleAttribute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scaling", meta=(PCG_Overridable, DisplayName="Arrive Scale", EditCondition="ArriveScaleInput==EPCGExInputValueType::Constant", EditConditionHides))
	double ArriveScaleConstant = 1;

	PCGEX_SETTING_VALUE_GET(ArriveScale, FVector, ArriveScaleInput, ArriveScaleAttribute, FVector(ArriveScaleConstant))

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scaling", meta=(PCG_NotOverridable))
	EPCGExInputValueType LeaveScaleInput = EPCGExInputValueType::Constant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scaling", meta=(PCG_Overridable, DisplayName="Leave Scale (Attr)", EditCondition="LeaveScaleInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector LeaveScaleAttribute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scaling", meta=(PCG_Overridable, DisplayName="Leave Scale", EditCondition="LeaveScaleInput==EPCGExInputValueType::Constant", EditConditionHides))
	double LeaveScaleConstant = 1;

	PCGEX_SETTING_VALUE_GET(LeaveScale, FVector, LeaveScaleInput, LeaveScaleAttribute, FVector(LeaveScaleConstant))
};

struct FPCGExWriteTangentsContext final : FPCGExPathProcessorContext
{
	friend class FPCGExWriteTangentsElement;

	UPCGExTangentsInstancedFactory* Tangents = nullptr;
	UPCGExTangentsInstancedFactory* StartTangents = nullptr;
	UPCGExTangentsInstancedFactory* EndTangents = nullptr;
};

class FPCGExWriteTangentsElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(WriteTangents)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExWriteTangents
{
	const FName SourceOverridesTangents = TEXT("Overrides : Tangents");
	const FName SourceOverridesTangentsStart = TEXT("Overrides : Start Tangents");
	const FName SourceOverridesTangentsEnd = TEXT("Overrides : End Tangents");


	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExWriteTangentsContext, UPCGExWriteTangentsSettings>
	{
		bool bClosedLoop = false;
		int32 LastIndex = 0;

		TSharedPtr<PCGExDetails::TSettingValue<FVector>> ArriveScaleReader;
		TSharedPtr<PCGExDetails::TSettingValue<FVector>> LeaveScaleReader;

		TSharedPtr<PCGExData::TBuffer<FVector>> ArriveWriter;
		TSharedPtr<PCGExData::TBuffer<FVector>> LeaveWriter;

		TSharedPtr<FPCGExTangentsOperation> Tangents;
		TSharedPtr<FPCGExTangentsOperation> StartTangents;
		TSharedPtr<FPCGExTangentsOperation> EndTangents;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;

		virtual void CompleteWork() override;
	};
}
