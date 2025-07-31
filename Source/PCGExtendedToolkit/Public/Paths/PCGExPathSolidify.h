// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"


#include "Sampling/PCGExSampling.h"
#include "PCGExPathSolidify.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/solidify"))
class UPCGExPathSolidifySettings : public UPCGExPathProcessorSettings
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

public:
	/** If the path is not closed, the last point cannot be solidified, thus it's usually preferable to remove it. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bRemoveLastPoint = true;

	/** Align the point to the direction over the selected axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExMinimalAxis SolidificationAxis = EPCGExMinimalAxis::X;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="SolidificationAxis != EPCGExMinimalAxis::None"))
	EPCGExInputValueType SolidificationLerpInput = EPCGExInputValueType::Constant;

	/** Solidification Lerp attribute (read from Edge).*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Solidification Lerp (Attr)", EditCondition="SolidificationLerpInput == EPCGExInputValueType::Attribute && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	FPCGAttributePropertyInputSelector SolidificationLerpAttribute;

	/** Solidification Lerp constant.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Solidification Lerp", EditCondition="SolidificationLerpInput == EPCGExInputValueType::Constant && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	double SolidificationLerpConstant = 0;

	PCGEX_SETTING_VALUE_GET(SolidificationLerp, double, SolidificationLerpInput, SolidificationLerpAttribute, SolidificationLerpConstant)

	// Edge radiuses

	/** Whether or not to write the point extents over the local X axis.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta=(PCG_Overridable, EditCondition="SolidificationAxis != EPCGExMinimalAxis::X && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	bool bWriteRadiusX = false;

	/** Type of Radius X value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta=(PCG_Overridable, EditCondition="bWriteRadiusX && SolidificationAxis != EPCGExMinimalAxis::X && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	EPCGExInputValueType RadiusXInput = EPCGExInputValueType::Constant;

	/** Attribute read on points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta = (PCG_Overridable, DisplayName="Radius X (Attr)", EditCondition="bWriteRadiusX && SolidificationAxis != EPCGExMinimalAxis::X && SolidificationAxis != EPCGExMinimalAxis::None && RadiusXInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector RadiusXSourceAttribute;

	/** Radius X Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta = (PCG_Overridable, DisplayName="Radius X", EditCondition="bWriteRadiusX && SolidificationAxis != EPCGExMinimalAxis::X && SolidificationAxis != EPCGExMinimalAxis::None && RadiusXInput == EPCGExInputValueType::Constant", EditConditionHides))
	double RadiusXConstant = 1;


	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta=(PCG_Overridable, EditCondition="SolidificationAxis != EPCGExMinimalAxis::Y && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	bool bWriteRadiusY = false;

	/** Type of Radius Y value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta=(PCG_Overridable, EditCondition="bWriteRadiusY && SolidificationAxis != EPCGExMinimalAxis::Y && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	EPCGExInputValueType RadiusYInput = EPCGExInputValueType::Constant;

	/** Attribute read on points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta = (PCG_Overridable, DisplayName="Radius Y (Attr)", EditCondition="bWriteRadiusY && SolidificationAxis != EPCGExMinimalAxis::Y && SolidificationAxis != EPCGExMinimalAxis::None && RadiusYInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector RadiusYSourceAttribute;

	/** Radius Y Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta = (PCG_Overridable, DisplayName="Radius Y", EditCondition="bWriteRadiusY && SolidificationAxis != EPCGExMinimalAxis::Y && SolidificationAxis != EPCGExMinimalAxis::None && RadiusYInput == EPCGExInputValueType::Constant", EditConditionHides))
	double RadiusYConstant = 1;


	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta=(PCG_Overridable, EditCondition="SolidificationAxis != EPCGExMinimalAxis::Z && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	bool bWriteRadiusZ = false;

	/** Type of Radius Z value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta=(PCG_Overridable, EditCondition="bWriteRadiusZ && SolidificationAxis != EPCGExMinimalAxis::Z && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	EPCGExInputValueType RadiusZInput = EPCGExInputValueType::Constant;

	/** Attribute read on points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta = (PCG_Overridable, DisplayName="Radius Z (Attr)", EditCondition="bWriteRadiusZ && SolidificationAxis != EPCGExMinimalAxis::Z && SolidificationAxis != EPCGExMinimalAxis::None && RadiusZInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector RadiusZSourceAttribute;

	/** Radius Z Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Radiuses", meta = (PCG_Overridable, DisplayName="Radius Z", EditCondition="bWriteRadiusZ && SolidificationAxis != EPCGExMinimalAxis::Z && SolidificationAxis != EPCGExMinimalAxis::None && RadiusZInput == EPCGExInputValueType::Constant", EditConditionHides))
	double RadiusZConstant = 1;
};

struct FPCGExPathSolidifyContext final : FPCGExPathProcessorContext
{
	friend class FPCGExPathSolidifyElement;
};

class FPCGExPathSolidifyElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(PathSolidify)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExPathSolidify
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExPathSolidifyContext, UPCGExPathSolidifySettings>
	{
		bool bClosedLoop = false;

		TSharedPtr<PCGExDetails::TSettingValue<double>> SolidificationLerp;

		TSharedPtr<PCGExPaths::FPath> Path;
		TSharedPtr<PCGExPaths::FPathEdgeLength> PathLength;

#define PCGEX_LOCAL_EDGE_GETTER_DECL(_AXIS) TSharedPtr<PCGExDetails::TSettingValue<double>> SolidificationRad##_AXIS;
		PCGEX_FOREACH_XYZ(PCGEX_LOCAL_EDGE_GETTER_DECL)
#undef PCGEX_LOCAL_EDGE_GETTER_DECL

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
	};
}
