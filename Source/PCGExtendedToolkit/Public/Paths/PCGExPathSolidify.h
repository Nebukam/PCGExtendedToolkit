// CopyCross 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"
#include "PCGExPaths.h"
#include "Details/PCGExSettingsMacros.h"

#include "Sampling/PCGExSampling.h"
#include "PCGExPathSolidify.generated.h"

namespace PCGExPaths
{
	class FPathEdgeAvgNormal;
}

namespace PCGExPaths
{
	class FPathEdgeLength;
	class FPath;
}

UENUM()
enum class EPCGExSolidificationSpace : uint8
{
	Local   = 0 UMETA(DisplayName = "Local", ToolTip="Solidifies in local space. Units ignore scale."),
	Scaled  = 1 UMETA(DisplayName = "Scaled", ToolTip="Solidifies accounting for scale. Units will account for scale."),
	Unscale = 2 UMETA(DisplayName = "Unscaled", ToolTip="Solidifies in local space, resets scale to 1"),
};

USTRUCT(BlueprintType)
struct FPCGExPathSolidificationAxisDetails
{
	GENERATED_BODY()
	virtual ~FPCGExPathSolidificationAxisDetails() = default;
	FPCGExPathSolidificationAxisDetails() = default;
	
	/** Input value type for flip */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, DisplayPriority=-1))
	EPCGExInputValueType FlipInput = EPCGExInputValueType::Constant;

	/** Whether to flip this axis or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, DisplayName=" └─ Flip", meta = (PCG_Overridable, EditCondition="FlipInput == EPCGExInputValueType::Constant", EditConditionHides, DisplayPriority=-1))
	bool bFlip = false;
	
	/** Whether to flip this axis or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, DisplayName=" └─ Flip (Attr)", meta = (PCG_Overridable, EditCondition="FlipInput != EPCGExInputValueType::Constant", EditConditionHides, DisplayPriority=-1))
	FName FlipAttributeName = NAME_None;
	
	PCGEX_SETTING_VALUE_DECL(Flip, bool)

	/** How to deal with scale during solidification */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, DisplayPriority=-1))
	//EPCGExSolidificationSpace Space = EPCGExSolidificationSpace::Local;

	virtual bool Validate(FPCGExContext* InContext) const;
};

USTRUCT(BlueprintType)
struct FPCGExPathSolidificationRadiusDetails : public FPCGExPathSolidificationAxisDetails
{
	GENERATED_BODY()

	FPCGExPathSolidificationRadiusDetails() = default;
	
	/** Input value type for Radius */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExInputValueToggle RadiusInput = EPCGExInputValueToggle::Disabled;
	
	/** Constant Radius for this axis */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, DisplayName=" └─ Radius", meta = (PCG_Overridable, EditCondition="RadiusInput == EPCGExInputValueToggle::Constant", EditConditionHides, ClampMin=0.001))
	double Radius = false;

	/** Attribute-driven radius for this axis */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, DisplayName=" └─ Radius (Attr)", meta = (PCG_Overridable, EditCondition="RadiusInput == EPCGExInputValueToggle::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector RadiusAttribute;

	PCGEX_SETTING_VALUE_DECL(Radius, double)

	virtual bool Validate(FPCGExContext* InContext) const override;
};

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
	virtual void ApplyDeprecation(UPCGNode* InOutNode) override;
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	virtual PCGExData::EIOInit GetMainDataInitializationPolicy() const override;

public:
	/** If the path is not closed, the last point cannot be solidified, thus it's usually preferable to remove it. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bRemoveLastPoint = true;

	/** Axis order. The first axis is aligned to the segment (Forward), second is Up, third is Cross (Right) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExAxisOrder SolidificationOrder = EPCGExAxisOrder::XZY;
	
	// - Constant vs attribute
	// - Attribute can be int as pick (with sanitization)
	// - Attribute can be vector as pick (with sanitization)

	/** Primary axis settings (direction aligned to the segment) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExPathSolidificationAxisDetails ForwardAxis;
	
	/** Up axis settings, relative to the selected order */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExPathSolidificationRadiusDetails NormalAxis;

	/** Cross axis settings, relative to the selected order */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExPathSolidificationRadiusDetails CrossAxis;

	/** How should the cross direction (Cross) be computed.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExInputValueType CrossDirectionType = EPCGExInputValueType::Constant;

	/** Fetch the cross direction vector from a local point attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cross Direction (Attr)", EditCondition="CrossDirectionType != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector CrossDirectionAttribute;

	/** Type of arithmetic path point cross direction.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Cross Direction", EditCondition="CrossDirectionType == EPCGExInputValueType::Constant", EditConditionHides))
	EPCGExPathNormalDirection CrossDirection = EPCGExPathNormalDirection::AverageNormal;

	/** Inverts cross direction.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Invert Direction", EditCondition="CrossDirectionType != EPCGExInputValueType::Constant", EditConditionHides))
	bool bInvertDirection = false;

#pragma region DEPRECATED

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

	PCGEX_SETTING_VALUE_DECL(SolidificationLerp, double)

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

#pragma endregion
};

struct FPCGExPathSolidifyContext final : FPCGExPathProcessorContext
{
	friend class FPCGExPathSolidifyElement;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
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

		TSharedPtr<PCGExDetails::TSettingValue<int32>> AxisOrder;
		TSharedPtr<PCGExDetails::TSettingValue<FVector2D>> AxisOrderVector;
		
		TSharedPtr<PCGExDetails::TSettingValue<double>> SolidificationLerp;

		TSharedPtr<PCGExDetails::TSettingValue<bool>> ForwardFlipBuffer;
		TSharedPtr<PCGExDetails::TSettingValue<double>> ForwardRadiusBuffer;
		
		TSharedPtr<PCGExDetails::TSettingValue<bool>> NormalFlipBuffer;
		TSharedPtr<PCGExDetails::TSettingValue<double>> NormalRadiusBuffer;

		TSharedPtr<PCGExDetails::TSettingValue<bool>> CrossFlipBuffer;
		TSharedPtr<PCGExDetails::TSettingValue<double>> CrossRadiusBuffer;

		TSharedPtr<PCGExPaths::FPath> Path;
		TSharedPtr<PCGExPaths::FPathEdgeLength> PathLength;
		TSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>> PathNormal;
		TSharedPtr<PCGExData::TBuffer<FVector>> CrossGetter;

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
