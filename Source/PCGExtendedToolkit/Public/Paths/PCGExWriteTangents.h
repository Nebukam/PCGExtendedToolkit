// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"


#include "Tangents/PCGExTangentsOperation.h"
#include "PCGExWriteTangents.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExWriteTangentsSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

	UPCGExWriteTangentsSettings(const FObjectInitializer& ObjectInitializer);

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(WriteTangents, "Path : Write Tangents", "Computes & writes points tangents.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	virtual FName GetPointFilterLabel() const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName ArriveName = "ArriveTangent";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName LeaveName = "LeaveTangent";

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault))
	TObjectPtr<UPCGExTangentsOperation> Tangents;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault, EditCondition="!bClosedLoop"))
	TObjectPtr<UPCGExTangentsOperation> StartTangents;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault, EditCondition="!bClosedLoop"))
	TObjectPtr<UPCGExTangentsOperation> EndTangents;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scaling", meta=(PCG_NotOverridable))
	EPCGExFetchType ArriveScaleType = EPCGExFetchType::Constant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scaling", meta=(PCG_Overridable, EditCondition="ArriveScaleType==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector ArriveScaleAttribute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scaling", meta=(PCG_Overridable, EditCondition="ArriveScaleType==EPCGExFetchType::Constant", EditConditionHides))
	double ArriveScaleConstant = 1;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scaling", meta=(PCG_NotOverridable))
	EPCGExFetchType LeaveScaleType = EPCGExFetchType::Constant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scaling", meta=(PCG_Overridable, EditCondition="LeaveScaleType==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector LeaveScaleAttribute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scaling", meta=(PCG_Overridable, EditCondition="LeaveScaleType==EPCGExFetchType::Constant", EditConditionHides))
	double LeaveScaleConstant = 1;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExWriteTangentsContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExWriteTangentsElement;

	UPCGExTangentsOperation* Tangents = nullptr;
	UPCGExTangentsOperation* StartTangents = nullptr;
	UPCGExTangentsOperation* EndTangents = nullptr;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExWriteTangentsElement final : public FPCGExPathProcessorElement
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

namespace PCGExWriteTangents
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExWriteTangentsContext, UPCGExWriteTangentsSettings>
	{
		bool bClosedLoop = false;
		int32 LastIndex = 0;

		TSharedPtr<PCGExData::TBuffer<FVector>> ArriveScaleReader;
		TSharedPtr<PCGExData::TBuffer<FVector>> LeaveScaleReader;
		FVector ConstantArriveScale = FVector::OneVector;
		FVector ConstantLeaveScale = FVector::OneVector;

		TSharedPtr<PCGExData::TBuffer<FVector>> ArriveWriter;
		TSharedPtr<PCGExData::TBuffer<FVector>> LeaveWriter;

		UPCGExTangentsOperation* Tangents = nullptr;
		UPCGExTangentsOperation* StartTangents = nullptr;
		UPCGExTangentsOperation* EndTangents = nullptr;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}
