// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"


#include "Sampling/PCGExSampling.h"
#include "PCGExPathSolidify.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPathSolidifySettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathSolidify, "Path : Solidify", "Solidify a path.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** If the path is not closed, the last point cannot be solidified, thus it's usually preferable to remove it. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bRemoveLastPoint = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bScaleBounds = true;

	/** Align the point to the direction over the selected axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExMinimalAxis SolidificationAxis = EPCGExMinimalAxis::X;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="SolidificationAxis != EPCGExMinimalAxis::None"))
	EPCGExFetchType SolidificationLerpOperand = EPCGExFetchType::Constant;

	/** Solidification Lerp constant.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="SolidificationLerpOperand == EPCGExFetchType::Constant && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	double SolidificationLerpConstant = 0;

	/** Solidification Lerp attribute (read from Edge).*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="SolidificationLerpOperand == EPCGExFetchType::Attribute && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	FPCGAttributePropertyInputSelector SolidificationLerpAttribute;

	// Edge radiuses

	/** Whether or not to write the point extents over the local X axis.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta=(PCG_Overridable, EditCondition="SolidificationAxis != EPCGExMinimalAxis::X && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	bool bWriteRadiusX = false;

	/** Type of Radius X value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta=(PCG_Overridable, EditCondition="bWriteRadiusX && SolidificationAxis != EPCGExMinimalAxis::X && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	EPCGExFetchType RadiusXType = EPCGExFetchType::Constant;

	/** Attribute read on points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta = (PCG_Overridable, EditCondition="bWriteRadiusX && SolidificationAxis != EPCGExMinimalAxis::X && SolidificationAxis != EPCGExMinimalAxis::None && RadiusXType==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector RadiusXSourceAttribute;

	/** Radius X Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta = (PCG_Overridable, EditCondition="bWriteRadiusX && SolidificationAxis != EPCGExMinimalAxis::X && SolidificationAxis != EPCGExMinimalAxis::None && RadiusXType==EPCGExFetchType::Constant", EditConditionHides))
	double RadiusXConstant = 1;


	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta=(PCG_Overridable, EditCondition="SolidificationAxis != EPCGExMinimalAxis::Y && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	bool bWriteRadiusY = false;

	/** Type of Radius Y value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta=(PCG_Overridable, EditCondition="bWriteRadiusY && SolidificationAxis != EPCGExMinimalAxis::Y && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	EPCGExFetchType RadiusYType = EPCGExFetchType::Constant;

	/** Attribute read on points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta = (PCG_Overridable, EditCondition="bWriteRadiusY && SolidificationAxis != EPCGExMinimalAxis::Y && SolidificationAxis != EPCGExMinimalAxis::None && RadiusZType==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector RadiusYSourceAttribute;

	/** Radius Y Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta = (PCG_Overridable, EditCondition="bWriteRadiusY && SolidificationAxis != EPCGExMinimalAxis::Y && SolidificationAxis != EPCGExMinimalAxis::None && RadiusYType==EPCGExFetchType::Constant", EditConditionHides))
	double RadiusYConstant = 1;


	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta=(PCG_Overridable, EditCondition="SolidificationAxis != EPCGExMinimalAxis::Z && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	bool bWriteRadiusZ = false;

	/** Type of Radius Z value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta=(PCG_Overridable, EditCondition="bWriteRadiusZ && SolidificationAxis != EPCGExMinimalAxis::Z && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	EPCGExFetchType RadiusZType = EPCGExFetchType::Constant;

	/** Attribute read on points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta = (PCG_Overridable, EditCondition="bWriteRadiusZ && SolidificationAxis != EPCGExMinimalAxis::Z && SolidificationAxis != EPCGExMinimalAxis::None && RadiusZType==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector RadiusZSourceAttribute;

	/** Radius Z Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta = (PCG_Overridable, EditCondition="bWriteRadiusZ && SolidificationAxis != EPCGExMinimalAxis::Z && SolidificationAxis != EPCGExMinimalAxis::None && RadiusZType==EPCGExFetchType::Constant", EditConditionHides))
	double RadiusZConstant = 1;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathSolidifyContext final : FPCGExPathProcessorContext
{
	friend class FPCGExPathSolidifyElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathSolidifyElement final : public FPCGExPathProcessorElement
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

namespace PCGExPathSolidify
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExPathSolidifyContext, UPCGExPathSolidifySettings>
	{
		TSharedPtr<PCGExData::TBuffer<double>> SolidificationLerpGetter;

#define PCGEX_LOCAL_EDGE_GETTER_DECL(_AXIS) TSharedPtr<PCGExData::TBuffer<double>> SolidificationRad##_AXIS; bool bOwnSolidificationRad##_AXIS = true; double Rad##_AXIS##Constant = 1;
		PCGEX_FOREACH_XYZ(PCGEX_LOCAL_EDGE_GETTER_DECL)
#undef PCGEX_LOCAL_EDGE_GETTER_DECL

		bool bClosedLoop = false;

		int32 LastIndex = 0;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}
