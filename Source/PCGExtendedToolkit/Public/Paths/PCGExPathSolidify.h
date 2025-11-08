// Copyright 2025 Timothé Lapetite and contributors
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
	EPCGExInputValueToggle FlipInput = EPCGExInputValueToggle::Disabled;

	/** Whether to flip this axis or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, DisplayName="Flip", meta = (PCG_Overridable, EditCondition="FlipInput == EPCGExInputValueToggle::Constant", EditConditionHides, DisplayPriority=-1))
	bool bFlip = false;
	
	/** Whether to flip this axis or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, DisplayName="Flip (Attr)", meta = (PCG_Overridable, EditCondition="FlipInput == EPCGExInputValueToggle::Attribute", EditConditionHides, DisplayPriority=-1))
	FName FlipAttributeName = NAME_None;
	
	PCGEX_SETTING_VALUE_DECL(Flip, bool)

	/** How to deal with scale during solidification */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, DisplayPriority=-1))
	//EPCGExSolidificationSpace Space = EPCGExSolidificationSpace::Local;

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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, DisplayName="Radius", meta = (PCG_Overridable, EditCondition="RadiusInput == EPCGExInputValueToggle::Constant", EditConditionHides, ClampMin=0.001))
	double Radius = 10;

	/** Attribute-driven radius for this axis */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, DisplayName="Radius (Attr)", meta = (PCG_Overridable, EditCondition="RadiusInput == EPCGExInputValueToggle::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector RadiusAttribute;

	PCGEX_SETTING_VALUE_DECL(Radius, double)

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

	/** Axis order. First axis will use the segment direction, second is the path normal. These are Primary > Secondary > Tertiary. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExAxisOrder SolidificationOrder = EPCGExAxisOrder::XYZ;

	/** Defines how the selected axis will be used to construct the point' rotation. This will be using remapped axis from the selected order. X = Primary, Y = Secondary, Z = Tertiary*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExMakeRotAxis RotationConstruction = EPCGExMakeRotAxis::XY;
	
	// - Constant vs attribute
	// - Attribute can be int as pick (with sanitization)
	// - Attribute can be vector as pick (with sanitization)

	/** Primary axis settings (direction aligned to the segment) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Primary"))
	FPCGExPathSolidificationAxisDetails PrimaryAxis;

	/** Secondary axis settings, relative to the selected order */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Secondary"))
	FPCGExPathSolidificationRadiusDetails SecondaryAxis;
	
	/** Tertiary axis settings, relative to the selected order */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Tertiary"))
	FPCGExPathSolidificationRadiusDetails TertiaryAxis;

	/** How should the cross direction (Cross) be computed.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExInputValueType NormalType = EPCGExInputValueType::Constant;

	/** Fetch the cross direction vector from a local point attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Normal (Attr)", EditCondition="NormalType != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector NormalAttribute;

	/** Type of arithmetic path point cross direction.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Normal", EditCondition="NormalType == EPCGExInputValueType::Constant", EditConditionHides))
	EPCGExPathNormalDirection Normal = EPCGExPathNormalDirection::AverageNormal;

	/** Inverts normal direction.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Invert Direction", EditCondition="NormalType != EPCGExInputValueType::Constant", EditConditionHides))
	bool bInvertDirection = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType SolidificationLerpInput = EPCGExInputValueType::Constant;

	/** Solidification Lerp attribute (read from Edge).*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Solidification Lerp (Attr)", EditCondition="SolidificationLerpInput == EPCGExInputValueType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector SolidificationLerpAttribute;

	/** Solidification Lerp constant.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Solidification Lerp", EditCondition="SolidificationLerpInput == EPCGExInputValueType::Constant", EditConditionHides))
	double SolidificationLerpConstant = 0;

	PCGEX_SETTING_VALUE_DECL(SolidificationLerp, double)
	
#pragma region DEPRECATED

	UPROPERTY()
	EPCGExMinimalAxis SolidificationAxis_DEPRECATED = EPCGExMinimalAxis::X;

	UPROPERTY()
	bool bWriteRadiusX_DEPRECATED = false;

	UPROPERTY()
	EPCGExInputValueType RadiusXInput_DEPRECATED = EPCGExInputValueType::Constant;

	UPROPERTY()
	FPCGAttributePropertyInputSelector RadiusXSourceAttribute_DEPRECATED;

	UPROPERTY()
	double RadiusXConstant_DEPRECATED = 1;

	UPROPERTY()
	bool bWriteRadiusY_DEPRECATED = false;

	UPROPERTY()
	EPCGExInputValueType RadiusYInput_DEPRECATED = EPCGExInputValueType::Constant;

	UPROPERTY()
	FPCGAttributePropertyInputSelector RadiusYSourceAttribute_DEPRECATED;

	UPROPERTY()
	double RadiusYConstant_DEPRECATED = 1;

	UPROPERTY()
	bool bWriteRadiusZ_DEPRECATED = false;

	UPROPERTY()
	EPCGExInputValueType RadiusZInput_DEPRECATED = EPCGExInputValueType::Constant;

	UPROPERTY()
	FPCGAttributePropertyInputSelector RadiusZSourceAttribute_DEPRECATED;

	UPROPERTY()
	double RadiusZConstant_DEPRECATED = 1;

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

		TSharedPtr<PCGExDetails::TSettingValue<bool>> PrimaryFlip;

		TSharedPtr<PCGExDetails::TSettingValue<bool>> SecondaryFlip;
		TSharedPtr<PCGExDetails::TSettingValue<double>> SecondaryRadius;
		
		TSharedPtr<PCGExDetails::TSettingValue<bool>> TertiaryFlip;
		TSharedPtr<PCGExDetails::TSettingValue<double>> TertiaryRadius;

		TSharedPtr<PCGExPaths::FPath> Path;
		TSharedPtr<PCGExPaths::FPathEdgeLength> PathLength;
		TSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>> PathNormal;
		TSharedPtr<PCGExData::TBuffer<FVector>> NormalGetter;

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
