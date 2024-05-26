// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Sampling/PCGExSampling.h"
#include "PCGExWriteEdgeExtras.generated.h"

#define PCGEX_FOREACH_FIELD_EDGEEXTRAS(MACRO)\
MACRO(EdgeLength, double)\
MACRO(EdgeDirection, FVector)

namespace PCGExDataBlending
{
	class FMetadataBlender;
	struct FPropertiesBlender;
}

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Edge Direction Mode"))
enum class EPCGExEdgeDirectionMethod : uint8
{
	EndpointsOrder UMETA(DisplayName = "Endpoints Order", ToolTip="Uses the edge' Start & End properties"),
	EndpointsIndices UMETA(DisplayName = "Endpoints Indices", ToolTip="Uses the edge' Start & End indices"),
	EndpointsAttribute UMETA(DisplayName = "Endpoints Attribute", ToolTip="Uses a single-component property or attribute value on Start & End points"),
	EdgeDotAttribute UMETA(DisplayName = "Edge Dot Attribute", ToolTip="Chooses the highest dot product against a vector property or attribute on the edge point"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Edge Direction Choice"))
enum class EPCGExEdgeDirectionChoice : uint8
{
	SmallestToGreatest UMETA(DisplayName = "Smallest to Greatest", ToolTip="Direction points from smallest to greatest value"),
	GreatestToSmallest UMETA(DisplayName = "Greatest to Smallest", ToolTip="Direction points from the greatest to smallest value")
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExWriteEdgeExtrasSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExWriteEdgeExtrasSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(WriteEdgeExtras, "Edges : Write Extras", "Extract & write extra edge informations to the point representing the edge.");
#endif

	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** Write normal from edges on vertices. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteVtxNormal = false;

	/** Name of the 'normal' vertex attribute to write normal to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteVtxNormal"))
	FName VtxNormalAttributeName = FName("Normal");

	/** Method to pick the edge direction amongst various possibilities.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExEdgeDirectionMethod DirectionMethod = EPCGExEdgeDirectionMethod::EndpointsOrder;

	/** Further refine the direction method. Not all methods make use of this property.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExEdgeDirectionChoice DirectionChoice = EPCGExEdgeDirectionChoice::SmallestToGreatest;

	/** Attribute picker for the selected Direction Method.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="DirectionMethod==EPCGExEdgeDirectionMethod::EndpointsAttribute", EditConditionHides))
	FPCGAttributePropertyInputSelector VtxSourceAttribute;

	/** Attribute picker for the selected Direction Method.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="DirectionMethod==EPCGExEdgeDirectionMethod::EdgeDotAttribute", EditConditionHides))
	FPCGAttributePropertyInputSelector EdgeSourceAttribute;

	/** Output Edge Length. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteEdgeLength = false;

	/** Name of the 'boolean' attribute to write sampling success to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteEdgeLength"))
	FName EdgeLengthAttributeName = FName("EdgeLength");

	/** Output Edge Direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteEdgeDirection = false;

	/** Name of the 'boolean' attribute to write sampling success to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteEdgeDirection"))
	FName EdgeDirectionAttributeName = FName("EdgeDirection");

	/** Edges will inherit point attributes*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta = (PCG_Overridable))
	bool bEndpointsBlending = false;

	/** Balance between start/end point ( When enabled, this value will be overriden by EdgePositionLerp, and Solidification, in that order. )*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bEndpointsBlending && !bWriteEdgePosition && SolidificationAxis == EPCGExMinimalAxis::None  )", ClampMin=0, ClampMax=1))
	double EndpointsWeights = 0.5;

	/** Defines how fused point properties and attributes are merged together. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bEndpointsBlending"))
	FPCGExBlendingSettings BlendingSettings = FPCGExBlendingSettings(EPCGExDataBlendingType::Average);

	/** Projection settings used for normal calculations. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionSettings ProjectionSettings;


	/** Update Edge position as a lerp between endpoints (according to the direction method selected above) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification", meta=(PCG_Overridable))
	bool bWriteEdgePosition = false;

	/** Position position lerp between start & end points*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification", meta=(PCG_Overridable, EditCondition="bWriteEdgePosition && SolidificationAxis == EPCGExMinimalAxis::None", ClampMin=0, ClampMax=1))
	double EdgePositionLerp = 0.5;

	/** Align the edge point to the edge direction over the selected axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification", meta = (PCG_Overridable))
	EPCGExMinimalAxis SolidificationAxis = EPCGExMinimalAxis::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification", meta=(PCG_Overridable, EditCondition="SolidificationAxis != EPCGExMinimalAxis::None"))
	EPCGExOperandType SolidificationLerpOperand = EPCGExOperandType::Constant;

	/** Solidification Lerp constant.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification", meta=(PCG_Overridable, EditCondition="SolidificationLerpOperand == EPCGExOperandType::Constant && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	double SolidificationLerpConstant = 0.5;

	/** Solidification Lerp attribute (read from Edge).*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification", meta=(PCG_Overridable, EditCondition="SolidificationLerpOperand == EPCGExOperandType::Attribute && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	FPCGAttributePropertyInputSelector SolidificationLerpAttribute;

	// Edge radiuses

	/** Whether or not to write the edge extents over the local X axis.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta=(PCG_Overridable, EditCondition="SolidificationAxis != EPCGExMinimalAxis::X && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	bool bWriteRadiusX = false;

	/** Source from which to fetch the Radius X value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta=(PCG_Overridable, EditCondition="bWriteRadiusX && SolidificationAxis != EPCGExMinimalAxis::X && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	EPCGExGraphValueSource RadiusXSource = EPCGExGraphValueSource::Point;

	/** Attribute read on edge endpoints */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta = (PCG_Overridable, EditCondition="bWriteRadiusX && SolidificationAxis != EPCGExMinimalAxis::X && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	FPCGAttributePropertyInputSelector RadiusXSourceAttribute;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta=(PCG_Overridable, EditCondition="SolidificationAxis != EPCGExMinimalAxis::Y && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	bool bWriteRadiusY = false;

	/** Source from which to fetch the Radius Y value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta=(PCG_Overridable, EditCondition="bWriteRadiusY && SolidificationAxis != EPCGExMinimalAxis::Y && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	EPCGExGraphValueSource RadiusYSource = EPCGExGraphValueSource::Point;

	/** Attribute read on edge endpoints */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta = (PCG_Overridable, EditCondition="bWriteRadiusY && SolidificationAxis != EPCGExMinimalAxis::Y && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	FPCGAttributePropertyInputSelector RadiusYSourceAttribute;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta=(PCG_Overridable, EditCondition="SolidificationAxis != EPCGExMinimalAxis::Z && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	bool bWriteRadiusZ = false;

	/** Source from which to fetch the Radius Z value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta=(PCG_Overridable, EditCondition="bWriteRadiusZ && SolidificationAxis != EPCGExMinimalAxis::Z && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	EPCGExGraphValueSource RadiusZSource = EPCGExGraphValueSource::Point;

	/** Attribute read on edge endpoints */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta = (PCG_Overridable, EditCondition="bWriteRadiusZ && SolidificationAxis != EPCGExMinimalAxis::Z && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	FPCGAttributePropertyInputSelector RadiusZSourceAttribute;

private:
	friend class FPCGExWriteEdgeExtrasElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExWriteEdgeExtrasContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExWriteEdgeExtrasElement;

	virtual ~FPCGExWriteEdgeExtrasContext() override;

	FPCGExGeo2DProjectionSettings ProjectionSettings;

	PCGExDataBlending::FMetadataBlender* MetadataBlender;

	PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_DECL)
	bool bWriteEdgePosition;
	double EdgePositionLerp;

	EPCGExEdgeDirectionMethod DirectionMethod;
	EPCGExEdgeDirectionChoice DirectionChoice;

	bool bSolidify;
	bool bEndpointsBlending;
	double EndpointsWeights;

	PCGEx::FLocalSingleFieldGetter* VtxDirCompGetter = nullptr;
	PCGEx::FLocalVectorGetter* EdgeDirCompGetter = nullptr;

	PCGEx::FLocalSingleFieldGetter* SolidificationLerpGetter = nullptr;
	PCGEx::FLocalSingleFieldGetter* SolidificationRadX = nullptr;
	PCGEx::FLocalSingleFieldGetter* SolidificationRadY = nullptr;
	PCGEx::FLocalSingleFieldGetter* SolidificationRadZ = nullptr;

	PCGEx::TFAttributeWriter<FVector>* VtxNormalWriter = nullptr;

	bool bAscendingDesired = true;
	double StartWeight = 0;
	double EndWeight = 1;
};

class PCGEXTENDEDTOOLKIT_API FPCGExWriteEdgeExtrasElement : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
